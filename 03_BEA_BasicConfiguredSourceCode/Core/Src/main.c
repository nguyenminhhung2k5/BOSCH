/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_28inch.h"
#include "dcm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
uint8_t uart3_receive;

CAN_HandleTypeDef hcan1;
CAN_HandleTypeDef hcan2;
CAN_TxHeaderTypeDef CAN1_pHeader;
CAN_RxHeaderTypeDef CAN1_pHeaderRx;
CAN_FilterTypeDef CAN1_sFilterConfig;
CAN_TxHeaderTypeDef CAN2_pHeader;
CAN_RxHeaderTypeDef CAN2_pHeaderRx;
CAN_FilterTypeDef CAN2_sFilterConfig;
uint32_t CAN1_pTxMailbox;
uint32_t CAN2_pTxMailbox;

volatile uint16_t NumBytesReq = 0;
volatile uint8_t  REQ_BUFFER  [4096];
volatile uint8_t  REQ_1BYTE_DATA;

uint16_t g_TemperatureSensorRawValue_u16[1];

uint8_t CAN1_DATA_TX[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t CAN1_DATA_RX[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t CAN2_DATA_TX[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t CAN2_DATA_RX[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

uint16_t Num_Consecutive_Tester;
uint8_t  Flg_Consecutive = 0;

unsigned int TimeStamp;
// maximum characters send out via UART is 30
char bufsend[30]="XXX: D1 D2 D3 D4 D5 D6 D7 D8  ";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void MX_CAN1_Setup();
void MX_CAN2_Setup();
void USART3_SendString(uint8_t *ch);
void PrintCANLog(uint16_t CANID, uint8_t * CAN_Frame);
void SID_22_Practice();
void SID_2E_Practice();
void SID_27_Practice();
void delay(uint16_t delay);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ======================================================
 * Biến Rolling Counter cho Node 1
 * Tăng từ 0x0 đến 0xF rồi quay lại 0x0
 * ====================================================== */
uint8_t node1_tx_counter = 0;
uint8_t latest_node2_val0 = 0;
uint8_t latest_node2_val1 = 0;
uint32_t last_can1_tx_time = 0;

/* ======================================================
 * Hàm tính Checksum theo chuẩn CRC-8 SAE J1850
 *   - Polynomial : 0x1D
 *   - Init value  : 0xFF
 *   - Final XOR   : 0xFF
 * Tham số:
 *   data   : con trỏ đến mảng byte cần tính
 *   length : số byte cần tính (thường là 6, từ byte 0->5)
 * Trả về : 1 byte Checksum
 * ====================================================== */
uint8_t CRC8_SAE_J1850(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFF;          /* Giá trị khởi tạo */
    uint8_t i, bit;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];          /* XOR byte dữ liệu vào CRC hiện tại */

        for (bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)      /* Nếu bit cao nhất = 1 */
            {
                crc = (crc << 1) ^ 0x1D;   /* Dịch trái rồi XOR với đa thức */
            }
            else
            {
                crc = (crc << 1);           /* Chỉ dịch trái */
            }
        }
    }

    return crc ^ 0xFF;           /* XOR cuối cùng để ra kết quả */
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Biến cờ báo hiệu có bản tin CAN1 mới đến (set trong ngắt CAN1_RX0) */
  extern volatile uint8_t g_CAN1_RxFlag;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  MX_CAN1_Setup();
  MX_CAN2_Setup();
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_TemperatureSensorRawValue_u16, 1);

  /* Khởi tạo phân hệ chẩn đoán UDS DCM */
  Dcm_Init();
  HAL_UART_Receive_IT(&huart3, &REQ_1BYTE_DATA, 1);

  /* Khởi tạo màn hình LCD 2.8 inch và giao diện giám sát CAN Monitor */
  LCD_Init();
  LCD_Init_UI();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  USART3_SendString((uint8_t *)"[READY] Node 1 (BEA Node & Diagnostic ECU) Initialized!\r\n");
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Xử lý định kỳ DCM (Security Access timers, LED PB0, UDS packet qua UART & CAN) */
    Dcm_MainFunction();

    /* ============================================================
     * [NODE 1 - COMMUNICATION]
     * 1. Lắng nghe bản tin 0x0A2 từ Node 2 (Trainer / Tester) qua CAN1
     * LƯU Ý QUAN TRỌNG: TUYỆT ĐỐI KHÔNG kiểm tra CRC trên bản tin 0x0A2.
     * Đề bài và bảng chấm điểm chỉ rõ: 0x0A2 không có CRC ở Byte 6.
     * ============================================================ */
    if (g_CAN1_RxFlag == 1)
    {
      g_CAN1_RxFlag = 0; /* Xoá cờ ngắt */

      /* Chỉ xử lý bản tin ID 0x0A2 */
      if (CAN1_pHeaderRx.StdId == 0x0A2)
      {
        /* Cập nhật giá trị mới nhất nhận từ Node 2 */
        latest_node2_val0 = CAN1_DATA_RX[0];
        latest_node2_val1 = CAN1_DATA_RX[1];

        /* In log bản tin 0A2 vừa nhận ra UART */
        PrintCANLog(CAN1_pHeaderRx.StdId, CAN1_DATA_RX);

        /* Cập nhật LCD giãn cách để không làm trễ chu kỳ phát 50ms của CAN */
        static uint32_t last_lcd_rx_time = 0;
        if (TimeStamp - last_lcd_rx_time >= 100)
        {
          last_lcd_rx_time = TimeStamp;
          LCD_DisplayCANLog(CAN1_pHeaderRx.StdId, CAN1_DATA_RX, 1);
        }
      }
    }

    /* ============================================================
     * [NODE 1 - COMMUNICATION]
     * 2. Phát bản tin 0x012 theo chu kỳ CỐ ĐỊNH 50ms (± 1ms)
     * Thỏa mãn tiêu chí: "Node 1 can send 0x012 on time (+- 1 millisecond)"
     * ============================================================ */
    uint32_t current_time = TimeStamp;
    if (current_time - last_can1_tx_time >= 50)
    {
      last_can1_tx_time += 50; /* Cố định chu kỳ 50ms, chống trôi thời gian (drift) */

      /* Byte 0, 1: Dữ liệu mới nhất từ Node 2 */
      CAN1_DATA_TX[0] = latest_node2_val0;
      CAN1_DATA_TX[1] = latest_node2_val1;

      /* Byte 2: Tổng Byte0 + Byte1 */
      CAN1_DATA_TX[2] = latest_node2_val0 + latest_node2_val1;

      /* Byte 3, 4, 5: Điền 0x00 */
      CAN1_DATA_TX[3] = 0x00;
      CAN1_DATA_TX[4] = 0x00;
      CAN1_DATA_TX[5] = 0x00;

      /* Byte 6: Checksum CRC-8 SAE J1850 (tính trên 6 byte đầu 0..5) */
      CAN1_DATA_TX[6] = CRC8_SAE_J1850(CAN1_DATA_TX, 6);

      /* Byte 7: Điền 0x00 theo đề bài */
      CAN1_DATA_TX[7] = 0x00;

      /* Gửi bản tin 0x012 qua CAN1 về Node 2 */
      HAL_CAN_AddTxMessage(&hcan1, &CAN1_pHeader, CAN1_DATA_TX, &CAN1_pTxMailbox);

      /* Cập nhật Rolling Counter */
      node1_tx_counter = (node1_tx_counter + 1) & 0x0F;

      /* In log bản tin 012 vừa gửi ra UART */
      PrintCANLog(CAN1_pHeader.StdId, CAN1_DATA_TX);

      /* Cập nhật LCD giãn cách */
      static uint32_t last_lcd_tx_time = 0;
      if (TimeStamp - last_lcd_tx_time >= 100)
      {
        last_lcd_tx_time = TimeStamp;
        LCD_DisplayCANLog(CAN1_pHeader.StdId, CAN1_DATA_TX, 0);
      }
    }

    /* Giả lập IG OFF -> IG ON: Chấp nhận bất kỳ nút nhấn hoặc hướng Joystick nào (BtnU, BtnA, BtnB, BtnC, BtnD, BtnM, PA0/Wakeup) */
    if (!BtnU || !BtnA || !BtnB || !BtnC || !BtnD || !BtnM || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
    {
      delay(20);
      USART3_SendString((uint8_t *)"IG OFF ");
      while (!BtnU || !BtnA || !BtnB || !BtnC || !BtnD || !BtnM || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);
      MX_CAN1_Setup();
      MX_CAN2_Setup();
      Dcm_ApplyIgnitionCycle();
      USART3_SendString((uint8_t *)"--> IG ON (Applied Configured CAN ID)\r\n");
      delay(20);
    }
  }

  memset(&REQ_BUFFER,0x00,4096);
  NumBytesReq = 0;

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_2TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_10TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : PC13 PC4 PC5 PC6
                           PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6
                          |GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_CAN2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 (Security Access Indicator LED-0) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void MX_CAN1_Setup()
{
    USART3_SendString((uint8_t *)"[BOOT] Setting up CAN1...\r\n");

    /* Dừng CAN1 trước nếu đang chạy để cho phép cấu hình lại */
    HAL_CAN_Stop(&hcan1);

    /* 1. Cấu hình Bộ lọc Filter cho CAN1 */
    CAN1_sFilterConfig.FilterBank = 0;                      // Sử dụng Filter Bank 0
    CAN1_sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;  // Chế độ Mask (mặt nạ)
    CAN1_sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT; // Độ dài bộ lọc 32-bit
    CAN1_sFilterConfig.FilterIdHigh = 0x0000;
    CAN1_sFilterConfig.FilterIdLow = 0x0000;
    CAN1_sFilterConfig.FilterMaskIdHigh = 0x0000;          // Mask = 0: Chấp nhận mọi ID (hoặc lọc riêng 0x0A2)
    CAN1_sFilterConfig.FilterMaskIdLow = 0x0000;
    CAN1_sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;// Đẩy dữ liệu nhận vào FIFO0
    CAN1_sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;// Kích hoạt bộ lọc
    CAN1_sFilterConfig.SlaveStartFilterBank = 14;           // Dành cho CAN2 nếu có dùng

    /* Cài đặt cấu hình Filter vào thanh ghi */
    if (HAL_CAN_ConfigFilter(&hcan1, &CAN1_sFilterConfig) != HAL_OK)
    {
        USART3_SendString((uint8_t *)"[ERR] CAN1 Filter Config Failed!\r\n");
    }

    /* Khởi động ngoại vi CAN1 */
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        USART3_SendString((uint8_t *)"[WARN] CAN1 Start retry...\r\n");
        HAL_CAN_ResetError(&hcan1);
        HAL_CAN_Start(&hcan1);
    }

    /* Bật ngắt khi có bản tin đến FIFO0 */
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

    /* 2. Cấu hình thông số bản tin phát (TX Header) của Node 1 */
    CAN1_pHeader.StdId = Dcm_GetCurrentCANID();              // Lấy CAN ID mới
    CAN1_pHeader.ExtId = 0x00;
    CAN1_pHeader.IDE = CAN_ID_STD;           // 11-bit Standard ID
    CAN1_pHeader.RTR = CAN_RTR_DATA;         // Khung dữ liệu (Data Frame)
    CAN1_pHeader.DLC = 8;                    // 8 bytes dữ liệu
    CAN1_pHeader.TransmitGlobalTime = DISABLE;

    USART3_SendString((uint8_t *)"[BOOT] CAN1 Setup OK (Mode: Node 1 Practical Board)!\r\n");
}

void MX_CAN2_Setup()
{
    USART3_SendString((uint8_t *)"[BOOT] Setting up CAN2...\r\n");

    /* Dừng CAN2 trước nếu đang chạy */
    HAL_CAN_Stop(&hcan2);

    /* Cấu hình Bộ lọc Filter cho CAN2 */
    CAN2_sFilterConfig.FilterBank = 14;                     // Filter bank cho CAN2 bắt đầu từ 14
    CAN2_sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    CAN2_sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    CAN2_sFilterConfig.FilterIdHigh = 0x0000;
    CAN2_sFilterConfig.FilterIdLow = 0x0000;
    CAN2_sFilterConfig.FilterMaskIdHigh = 0x0000;
    CAN2_sFilterConfig.FilterMaskIdLow = 0x0000;
    CAN2_sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    CAN2_sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
    CAN2_sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan2, &CAN2_sFilterConfig) != HAL_OK)
    {
        USART3_SendString((uint8_t *)"[ERR] CAN2 Filter Config Failed!\r\n");
    }

    if (HAL_CAN_Start(&hcan2) != HAL_OK)
    {
        USART3_SendString((uint8_t *)"[ERR] CAN2 Start Failed!\r\n");
    }

    if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        USART3_SendString((uint8_t *)"[ERR] CAN2 ActivateNotification Failed!\r\n");
    }

    /* Cấu hình TX Header CAN2 */
    CAN2_pHeader.StdId = 0x0A2;
    CAN2_pHeader.ExtId = 0x00;
    CAN2_pHeader.IDE = CAN_ID_STD;
    CAN2_pHeader.RTR = CAN_RTR_DATA;
    CAN2_pHeader.DLC = 8;
    CAN2_pHeader.TransmitGlobalTime = DISABLE;

    USART3_SendString((uint8_t *)"[BOOT] CAN2 Setup OK!\r\n");
}

void USART3_SendString(uint8_t *ch)
{
    uint16_t len = 0;
    while (ch[len] != 0) len++;
    HAL_UART_Transmit(&huart3, ch, len, HAL_MAX_DELAY);
}

void PrintCANLog(uint16_t CANID, uint8_t * CAN_Frame)
{
	char log_buf[80];
	sprintf(log_buf, "%u %03X: %02X %02X %02X %02X %02X %02X %02X %02X \r\n",
			TimeStamp, CANID,
			CAN_Frame[0], CAN_Frame[1], CAN_Frame[2], CAN_Frame[3],
			CAN_Frame[4], CAN_Frame[5], CAN_Frame[6], CAN_Frame[7]);
	USART3_SendString((uint8_t *)log_buf);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART3)
	{
		if (NumBytesReq < sizeof(REQ_BUFFER) - 1)
		{
			REQ_BUFFER[NumBytesReq++] = REQ_1BYTE_DATA;
		}
		HAL_UART_Receive_IT(&huart3, (uint8_t *)&REQ_1BYTE_DATA, 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART3)
	{
		/* Xóa các cờ lỗi phần cứng Overrun, Noise, Framing */
		__HAL_UART_CLEAR_OREFLAG(huart);
		__HAL_UART_CLEAR_NEFLAG(huart);
		__HAL_UART_CLEAR_FEFLAG(huart);
		__HAL_UART_CLEAR_PEFLAG(huart);
		/* Tiếp tục nhận ngắt 1 byte tiếp theo */
		HAL_UART_Receive_IT(&huart3, (uint8_t *)&REQ_1BYTE_DATA, 1);
	}
}
void delay(uint16_t delay)
{
	HAL_Delay(delay);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  USART3_SendString((uint8_t *)"\r\n[CRITICAL ERROR] Error_Handler entered!\r\n");
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
