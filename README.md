# FastEventServer

Server program written in C++.

It reads settings from `service.cfg`, and drives the output based on requests coming into the specified UDP port.

## Running

FastEventServer may be run from any terminal emulator (Terminal.app, Cmd.exe etc.).

## service.cfg

The functioning of the FastEventServer is based on this file. This is virtually a JSON file with entries as specified below:

```json
{
  "port": 11666,
  "driver": "leonardo",
  "options": {
    "port": "/dev/tty.usbmodem1421"
  }
}
```

- `port`: the UDP port the server listens to
- `driver`: the type of the driver to be used. Currently, there are four driver types:
  1. `leonardo`: the serial connection that can be accessed without a delay after plugging (e.g. Arduino Leonardo, Arduino micro, or an Arduino Uno flashed with [arduino-fasteventtrigger](https://github.com/gwappa/arduino-fasteventtrigger).
  2. `uno`: the serial connection that requires a delay after plugging, before being able to be used (e.g. Arduino Uno).
  3. `dummy`: a driver that does nothing.
  4. `verbose-dummy`: same as the `dummy` driver, except that it outputs the processed requests.
- `options`: the driver-specific option(s). Entries that do not fit with the current driver will be simply ignored.
  1. `port`: in case of using serial-port driver, the identifier to the serial port must be set here (e.g. `"/dev/tty.usbmodem...."` for *NIX-type systems, or `"COMx"` for Windows systems.

## Building

### 1. Building on \*NIX

```
$ cd server
$ make
```

### 2. Building on Windows

```
$ cd server
$ build.bat
```

## Profiling/Testing

### 1. Python-based API testing

`feclient` module is here in this directory. It is meant to bypass the (heavier) jAER and to call the FastEventServer API directly.

- `feclient.Session()` creates a connection with the server based on the settings on `service.cfg`. `session.send(ch)` sends a command to the server, and returns the command index.

- The `feclient.Command` class holds the available commands to be requested. In fact, the commands are not verified for the moment, and especially `Command.QUIT` may not work properly...

- A quick-and-dirty latency testing may be performed using the `feclient.test.latency` module.

  ```python
  python -m feclient.test.latency --help
  ```

  will output more information on how to use it.

- A serverless session may be also possible by using the `feclient.mock.server` module. Refer to the output of `python -m feclient.mock.server --help` again for more detailed description.

### 2. C++-based driver profiling

You can profile the read-write latency for your driver using the `profile_direct` binary. It produces a CSV file of sent/received timestamps, using the nanosecond-level system clock.

```bash
make profile_direct # building

./profile_direct > `date "+prof_%Y-%m-%d-%H%M%S.csv"` # writing to a time-stamped file

```

## Adding your own driver

### 1. Implementation

Any driver must implement the `fastevent::OutputDriver` interface (declared in `include/driver.h`), which requires any subclass to  have the following members:

1. `static std::string identifier()`: used to specify the driver, just like `uno` or `leonardo`.
2. `static ks::Result<fastevent::OutputDriver> setup(fastevent::Config& cfg)`: used to create an instance of the driver, including any necessary initialization procedures. `cfg` contains the JSON configuration parsed from `service.cfg`.
3. `void update(const char& out)`: update the status of the output based on the `out` byte.
4. `void shutdown()`: the hook for finalizing your driver instance. 
   **IMPORTANT NOTE**: because of the current implementation (and because of the nature of the UDP communication), this method may not be always called. Please do not count on this method to be called.

For a working example, please refer to the source code such as `src/lib/arduinodriver.cpp`.

### 2. Registration

For the moment, registration process of the drivers are done manually by hard-coding `src/main.cpp`. Add the following line of code:

```c++
registerOutputDriver(fastevent::driver::DummyDriver);
registerOutputDriver(fastevent::driver::VerboseDummyDriver);
registerOutputDriver(fastevent::driver::UnoDriver);
registerOutputDriver(fastevent::driver::LeonardoDriver);
registerOutputDriver(/* your driver class here */);
```

If you intend to use `profile_direct`, you need to do the same procedure for `src/profile_direct.cpp`, too.



