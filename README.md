# FastEventServer

Server program written in C++.

## Building on \*NIX

```
$ cd  server
$ make
```

## Building on Windows

```
$ cd server
$ build.bat
```


## Python-based testing (may be outdated)

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


