Data chunks:

Amazon AWS also allows "Range" parameter to download a specific set of data
https://docs.aws.amazon.com/AmazonS3/latest/API/API_GetObject.html#API_GetObject_RequestParameters

Youtube video about building media player: https://youtu.be/f8EGZa32Mts?si=oiKKXpinsHm0v6yc

recv, how to properly read the HTTP request
https://stackoverflow.com/questions/49821687/how-to-determine-if-i-received-entire-message-from-recv-calls


http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png

curl http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png  -H "Authorization: as"

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as"

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" > upload_test_1

dd if=/dev/urandom of=upload_test bs=1M count=1000

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" > upload_test_1

dd if=/dev/urandom of=upload_test bs=1M count=1000

diff -q upload_test test1.c

-------------------


## Correct test case:

head:
curl -v -I http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png

Large binary files:
dd if=/dev/urandom of=upload_test bs=1M count=1000
curl http://localhost:8082/tmp/upload_test t -H "Authorization: as" > upload_test_1
diff -q upload_test test1.c

## error case:
curl -i http://localhost:8082/home/theprophet/Pictures/Screenshots/test1.png  -H "Authorization: as"


