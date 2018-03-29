# FastEventServer

Server program written in C++.

You can also use the client program (TestClient) for profiling.

## Building on \*NIX

Server:

```
$ cd  server
$ g++ -Iinclude src/common/*.cpp src/service/*.cpp -o FastEventServer
```

## Building on Windows

Server:

```
$ cd server
$ cl /Iinclude /Wall /EHsc /FeFastEventServer src\common\*.cpp src\service\*.cpp Ws2_32.lib
```

## Testing

### Python-based testing

`client.py` module is available (python>=3.3 and numpy are required).

For example, you can test generating 1000 random commands to the server by following the steps below:

1. Run `FastEventServer` in a terminal emulator.
2. Open another terminal emulator (in the `server` directory) and enter following commands:
   ```
   >>> from client import Client
   >>> cl = Client(timed=True) # by doing this, you can enable latency calculation
   >>> cl.random(1000) # specify the number of commands to be generated
   >>> cl.close() # when you shut down the client, the latency stats will be displayed
   >>> numpy.array(cl.latency) # retrieve latency values in microseconds
   ```
More details may be found in `client.Client`'s docstring.

### C++-based testing (\*NIX-only)

C++-based testing approach is only available in \*NIX environment, but you can vary the frequency of
command generation.

Building:
```
$ cd  server
$ g++ -Iinclude src/common/*.cpp src/test/*.cpp -o TestClient
```

You can use `TestClient` by following the steps below:

1. Edit `testclient.cfg` in the `server` directory.
2. Run `FastEventServer` in a terminal emulator.
3. Open another terminal emulator (in the `server` directory) and run `./TestClient`.


