// ランダムな値を3秒毎に取得し、取得した値をファイルに書き込む。
// 取得した値が5回連続で基準値外だった場合、エラーメールを通知し処理を終了する。
// libcurlライブラリを使用し、Gmail SMTPサーバーにメールを送信する。

#include <stdio.h> 
#include <stdbool.h> 
#include <stdlib.h>
#include <time.h> 
#include <unistd.h> 
#include <string.h>
#include <curl/curl.h>

#define FROM    "<sample@gmail.com>" // 送信先のメールアドレス
#define TO      "<sample@gmail.com>" // 送信元のメールアドレス

int get_value();
int send_mail();

static const char *payload_text;

struct upload_status {
  size_t bytes_read;
};
 
static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp) {
  struct upload_status *upload_ctx = (struct upload_status *)userp;
  const char *data;
  size_t room = size * nmemb;
 
  if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
    return 0;
  }
 
  data = &payload_text[upload_ctx->bytes_read];
 
  if(data) {
    size_t len = strlen(data);
    if(room < len)
      len = room;
    memcpy(ptr, data, len);
    upload_ctx->bytes_read += len;
 
    return len;
  }
 
  return 0;
}

int main() {
  // 基準値（-15~45）
  const int MAX_VALUE = 45;
  const int MIN_VALUE = -15;

  FILE *log_file;

  int values[5];
  // 配列を範囲外の値で初期化
  for(int i = 0; i < 5; i++) {
    values[i] = 99; 
  }

  while(1) {
    // -20~50の範囲からランダムな数値を取得
    int value;
    value = get_value();
    printf("%d\n", value);

    // 現在日時を取得
    char date[64];
    time_t t = time(NULL);
    strftime(date, sizeof(date), "%Y/%m/%d %a %H:%M:%S", localtime(&t));

    // ログファイルに値を出力
    log_file = fopen("log_file.txt", "a");
    fprintf(log_file, "%s %d\n", date, value);
    fclose(log_file);

    // 取得した最新の値を配列に追加
    int new_value = value;
    for(int i = 4; i >= 0; i--) {
      int current_value = *(values + i);
      *(values + i) = new_value;

      new_value = current_value;
    }

    // 5回連続で基準値外の値だった場合、エラーを通知し処理終了
    if(values[0] <= 50 && values[0] >= -20) {
      bool is_error = true;
      for(int i = 0; i < 5; i++) {
        if(values[i] <= MAX_VALUE && values[i] >= MIN_VALUE) {
          is_error = false;
          break;
        }
      }

      if(is_error) {
        // メールヘッダ・メール本文を作成
        char mail_text[160];
        char template[] = 
          "To: " TO "\r\n"
          "From: " FROM "\r\n"
          "Subject: Error notification\r\n"
          "\r\n" 
          "The value becomes out of range.\r\n"
          "The latest value was";

        sprintf(mail_text, "%s %d.", template, value);
        payload_text = mail_text;

        // エラーを通知
        int res = send_mail();
            
        return res;
      }

    }

    sleep(3);
  }

  return 0;
}

// ランダムな数値を-20~50の範囲で生成し返却する
int get_value() { 
  int value;
  srand((unsigned int)time(NULL));
  value = -20 + rand() % 71;

  return value;
}

// メールを送信する
int send_mail() {
  CURL *curl;
  CURLcode res = CURLE_OK;
  struct curl_slist *recipients = NULL;
  struct upload_status upload_ctx = { 0 };

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, "sample@gmail.com"); // ユーザー名
    curl_easy_setopt(curl, CURLOPT_PASSWORD, "password"); // パスワード
    curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587"); // メールサーバーURL

    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM);

    recipients = curl_slist_append(recipients, TO);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    res = curl_easy_perform(curl);

    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
  }

  return (int)res;
}
