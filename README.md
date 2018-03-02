# FastEventServer

Server program written in C++.

You can also use the client program (TestClient) for profiling.

## Building on \*NIX

Server:

```
$ cd  server
$ g++ -Iinclude src/common/*.cpp src/service/*.cpp -o FastEventServer
```

Test client:
```
$ cd  server
$ g++ -Iinclude src/common/*.cpp src/test/*.cpp -o TestClient
```


## Building on Windows

Server:

```
$ cd server
$ cl /Iinclude /Wall /EHsc /FeFastEventServer src\common\*.cpp src\service\*.cpp Ws2_32.lib
```

Currently we do not provide a test client in Windows (because of apparent lack of nanosleep()).
