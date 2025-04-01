# 作業大綱

這個作業主要是使用C++ socket與pthread製作一個簡單的TCP聊天系統，其中包含`server.cpp`與`client.cpp`兩個主要程式
- `server.cpp`負責管理多個客戶端的連線，並建構whiteboard儲存與傳送訊息
- `client.cpp`讓使用者發送訊息至伺服器，並接收來自其他使用者的訊息

## 程式邏輯

### `server.cpp`

1. 建立socket：server.cpp透過`socket()`建立 TCP 連線，並使用`bind()`與`listen()`讓客戶端可以連線
2. 多執行緒客戶端：
   - 當新的客戶端連線時，伺服器會創建新的pthread執行緒。
   - 伺服器使用`pthread_mutex_t`互斥鎖的概念來確保所有執行緒能夠同步地存取whiteboard，以避免race condition與deadlock
3. 訊息儲存與轉發：
   - 當客戶端傳送訊息時，伺服器會將訊息儲存至whiteboard。
   - 使用`printWhiteboard()`來顯示當前聊天內容、傳送時間、傳送對象與發送者，並將訊息傳給傳送對象
4. 記錄每位用戶的訊息，如
   - string username記錄每位用戶的使用者名稱
   - int socket_fd記錄伺服器的監聽socket
   - sockaddr_in address記錄用戶地址
   - bool is_online記錄用戶上線狀態

### `client.cpp`

1. 連線至伺服器：
   - `client.cpp`透過`socket()`連線至`server.cpp`。
   - 連線成功後，客戶端可以收發訊息。
2. 訊息傳輸：
   - 當使用者輸入訊息後，會執行`send()`來將訊息發送至伺服器。
   - 伺服器收到後，會轉傳訊息至目標客戶端，確保雙向溝通順暢。
3. 多執行緒處理I/O：
   - 為了讓客戶端能夠同時發送與接收訊息，`client.cpp`使用pthread來分別處理使用者輸入與伺服器回應。

## 檔案內容

- `server.cpp` - 伺服器端程式，負責管理連線與訊息轉發和白板讀寫。
- `client.cpp` - 客戶端程式，讓使用者傳送與接收訊息。
- `makefile` - 定義編譯與執行程式的指令。
- `README.md` - 說明程式架構與使用方式。

## 使用方式

### 1. 編譯程式

在專案目錄下執行以下指令：

```sh
make
```

此指令會編譯`server.cpp`與`client.cpp`，生成`server`和`client`之執行檔。

### 2. 啟動伺服器

在第一個終端機A中執行：

```sh
./server
```

### 3. 啟動客戶端

在別的終端機B, C, D中執行以下指令來啟動多個客戶端：

```sh
./client [IP] [port] [username]
```

### 4. 進行聊天

- 發送訊息給特定使用者：
  ```sh
  chat 傳送對象 "傳送內容!"
  ```
- 若目標使用者離線，會顯示該用戶已off-line之訊息
- 若目標使用者未曾存在過，則會說該用戶not exist
- 使用者輸入`bye`可登出

---

## 開發心得

整體架構設計強調thread safety，確保多個使用者能夠同時存取白板不衝突。然而，在實作過程中，我經常遇到自己鎖住自己的情況，導致程式無法正常輸出或寫入白板。這需要我一步一步檢查，在每個可能的衝突點加上debug訊息來追蹤錯誤來源，這也讓我體會到多執行緒開發的複雜性與挑戰。
還有在呼叫每個函式的時候，因為不知道哪個函式會出問題，所以都會需要一個if來檢查該函式是否會正常運作，回傳值是否小於零等，不然很難找到出錯的地方。
這次作業讓我更加熟悉了C++ socket programming以及pthread多執行緒同步，儘管過程充滿挑戰，但最終還是成功做出了一個TCP聊天系統，十分有成就感。

