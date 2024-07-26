Data chunks:

Amazon AWS also allows "Range" parameter to download a specific set of data
https://docs.aws.amazon.com/AmazonS3/latest/API/API_GetObject.html#API_GetObject_RequestParameters

Youtube video about building media player: https://youtu.be/f8EGZa32Mts?si=oiKKXpinsHm0v6yc

recv, how to properly read the HTTP request
https://stackoverflow.com/questions/49821687/how-to-determine-if-i-received-entire-message-from-recv-calls





-------------------


## Correct test case:


#### head

```
curl -v -I http://localhost:8082/tmp/upload_test
```


#### Large binary files:

```
dd if=/dev/urandom of=upload_test bs=1M count=1000
curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" > upload_test_1
diff -q upload_test test1.c
```


## error case:

```
curl -i http://localhost:8082/home/theprophet/Pictures/Screenshots/test1.png  -H "Authorization: as"
```


----------------------

## Partial file

Create a 2Kib file and then issue curl request of 1Kib and append them

```
dd if=/dev/urandom of=upload_test bs=1K count=2

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" -H "Range: bytes=0-1023" >> upload_test_1

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" -H "Range: bytes=1024-2048" >> upload_test_1

cmp upload_test upload_test_1
```

#### 200MB file, chunks of 100MB

```
rm -rf upload_test_1

dd if=/dev/urandom of=upload_test bs=1M count=200

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" -H "Range: bytes=0-100000000" >> upload_test_1

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" -H "Range: bytes=100000001-200000000" >> upload_test_1

curl http://localhost:8082/tmp/upload_test  -H "Authorization: as" -H "Range: bytes=200000001-300000000" >> upload_test_1

cmp upload_test upload_test_1
```


#### Actual file test (Test PNG)

```
cd /home/theprophet/Pictures/Screenshots

curl -v -I http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png

rm -rf upload_test_1.png

curl  http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png -H "Authorization: as" -H "Range: bytes=0-50000" >> upload_test_1.png

curl http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png  -H "Authorization: as" -H "Range: bytes=50001-100000" >> upload_test_1.png

curl http://localhost:8082/home/theprophet/Pictures/Screenshots/test.png  -H "Authorization: as" -H "Range: bytes=100001-150000" >> upload_test_1.png

cmp test.png upload_test_1.png
```

#### Running client

```
python client.py http://localhost:8082/tmp/upload_test /tmp/upload_test /tmp/upload_test_2 102400
```