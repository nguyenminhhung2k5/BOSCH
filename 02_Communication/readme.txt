README - Communication Practice Diagram
=========================================

File: practice_diagram.drawio

What is this file?
-------------------
This is a flowchart made with draw.io (also called diagrams.net) that shows
how two nodes (devices) exchange CAN messages with each other, including a
checksum and counter check for validating the received data.

How to open it
---------------
1. Go to https://app.diagrams.net (or install the desktop draw.io app).
2. Choose "Open Existing Diagram" and select "practice_diagram.drawio".
3. You can also open it directly in VS Code if you have the
   "Draw.io Integration" extension installed.

What the diagram shows
-----------------------
There are two nodes in the diagram: "Node1" (right side) and "Node 2" (left
side). They send messages back and forth to each other:

1. Node 2 -> Node1: Message 0x0A2
   - data[0] = 1
   - data[1] = 2
   - data[2-5] = 0
   - data[6] = counter
   - data[7] = checksum

2. Node1 receives the message and follows this flow:
   - Read data from the message.
   - Check if checksum and counter are correct (decision/diamond box).
     - If "No" -> Discard the message.
     - If "Yes" -> Print the received data to the terminal, then prepare
       the data to send back:
         send_data[0] = receive_data[0]
         send_data[1] = receive_data[1]
         send_data[2] = receive_data[1] + receive_data[2]
         send_data[3-5] = 0
         send_data[6] = calculated_counter
         send_data[7] = calculated_checksum
   - Print the send data to the terminal, then send the data (this becomes
     message 0x12) back to Node 2.

3. Node1 -> Node 2: Message 0x12
   - data[0] = 1
   - data[1] = 2
   - data[2] = data[1] + data[0] = 3
   - data[3-5] = 0
   - data[6] = counter
   - data[7] = checksum

4. Node 2 receives the reply and repeats a similar flow:
   - Read data from the message.
   - Check if checksum and counter are correct.
     - If "No" -> Discard.
     - If "Yes" -> Print the received data to the terminal, prepare a new
       send message (calculate checksum and counter), print the send data,
       and send it back to Node1.

Purpose / Learning Goal
------------------------
This exercise is meant to helpThì anh mong là nó sẽ work như cái bài tester thì nó sẽ work như là cái như này. Anh nhấn như này thì nó sẽ tự nó request response đủ ha. Ví dụ 1 2 4 là read nhiệt độ này, rồi 26 đây là bao nhiêu... 26 này... 66... 38 độ gì đấy. Thì mấy đứa phải tự làm tính toán ha. CAN ID thì không nói rồi. Đấy, và anh mong là anh nhấn liên tục như thế này thì nó vẫn response được nhé. Bởi vì nó sẽ có một cái frame mà như cái hình ban đầu anh đưa mấy đứa ấy, nó sẽ có một cái frame như thế này và mấy đứa xử lý không tốt, nếu mà không clear buffer sau khi xử lý xong ấy, thì nó rất dễ bị... bị buffer tràn thì không không hẳn mà là nó dễ bị bị nhiễu bởi data phía trước. Thì... thì rõ ràng là nó sẽ xử lý không có tốt và anh mong ấy là đấy, nhấn liên tọi như thế này luôn mà nó không có vấn đề gì cả, như thế này. Đấy.

Còn phần... cái đây... phần flash... cái phần mà phần config sẵn của thằng này này, thì anh nhớ là nó không có cái sẵn như thế này đâu. Cái chân P0 cho thằng security access ấy, chân flash này này, mấy đứa phải tự config nhé. Đừng có hỏi tại sao nhấn sang config xong cái nó lại không chạy cái... chân P0 nó không lên được ấy.

Đây, ví dụ security access. Trong header đây là 5 giây đúng không? 5 giây thì bật lên phát 5 giây sau... đây nó tắt xuống. Mất security thì lúc đấy anh request nó sẽ trả về NRC là security access denied như thế này. Còn nếu mà anh pass security anh qua như thế này, OK này, nó sẽ là positive response như thế này. Kiểu thế.

Sau khi read lại cũng không read được đâu, bởi vì là em phải đổi cái... reset nó lại bằng một cái nút user đây này, nhấn phát on off như thế này. Read lại thì nó sẽ work quay lại được ID mới như thế này.

Thì... cái phần chắc là anh sẽ phải làm một cái phần video anh test tủng tưng bừng cái này lên. Cái security access ở đây thì anh đang làm nó không giống như cái đề... cái đây là cái đề trước rồi... là tận mười mấy byte cơ, mấy đứa chỉ cần làm 6 byte thôi. Bạn nào không làm được secure follow control thì 4 byte là được, là đủ, 4 byte seed key ấy, là đủ nhé. Còn đây là tới mười mấy byte, mười bảy, mười tám byte gì đấy, thì nó hơi dài. Thì cái này theo cái đề cũ rồi.

Mà thôi anh nghĩ là mấy bạn làm được thôi. Bách Khoa cộng với lại AI thì anh nghĩ sẽ OK thôi. Mấy mua... mua Claude chưa, mua mấy con mới nhất chưa? Hồi... hồi anh... còn học thì nó... nó hơi cà chớn hơn, hồi anh học thì... hồi đó không biết AI là cái gì đâu. Đang học thôi, học về xử lý ảnh, xử lý đồ này nọ thôi. Tầm này thì gen AI nó mạnh quá trời. students practice:
- Building and reading CAN message data frames.
- Calculating and validating a checksum and a rolling counter.
- Implementing a request/response (ping-pong) communication loop between
  two nodes.

Tips for students
------------------
- Follow the arrows in order to understand the message flow between the
  two nodes.
- Pay close attention to how the checksum and counter are calculated on
  the sending side and validated on the receiving side - this is the core
  logic you need to implement.
- The diamond (rhombus) shapes are decision points ("Yes"/"No") - make
  sure your code handles both the valid and invalid data cases.
