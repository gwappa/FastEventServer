# FastEventServer

A server program for the FastEvent project written in C++.

It reads settings from `service.cfg`, and drives the output based on requests coming into the specified UDP port.

## Prerequisites for running the server

You need to prepare two things before you can use the program.

### 1. Available output drivers

FastEventServer works with an "output driver" that is supposed to generate output triggers.

For the moment, we provide two Arduino-based output drivers:

1. the sample Arduino code (found in the `SampleDevice` directory): this driver is intended for a simple testing of the service,
   without an serious consideration on fast output latency.
   You can install the code on your Arduino through the Arduino IDE.
   The selection of the driver would be either `leonardo` or `UNO`, depends on your Arduino (see below).
2. The `arduino-fasteventtrigger` code: you can find it in [this repository](https://github.com/gwappa/arduino-fasteventtrigger).
   You need to put your UNO to the [DFU mode](https://www.arduino.cc/en/Hacking/DFUProgramming8U2) in order to use its USB-serial chip.
   Consequently, **you cannot use your UNO as an "Arduino" anymore** unless you program the firmware back into the chip (but it is still possible).

If you don't have any Arduino at hand, or you want to just test the program without any output generation, you can use the "dummy" or "verbose-dummy" drivers (see below). In essence, they emulate the output generation but do nothing in reality (and hence you cannot test the real output latency)

In case you want to use your DAQ as the output driver, there is an option to implement your own (see instructions below).

### 2. Writing "service.cfg"

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
  1. `port`: in case you use a serial-port driver, the identifier to the serial port must be set here
     (e.g. `"/dev/tty.usbmodem...."` for \*NIX-type systems, or `"COMx"` for Windows systems).

## Running the program

FastEventServer may be run from any terminal emulator (Terminal.app, Cmd.exe etc.).

### 1. Running on a Linux PC

Tested on Ubuntu 18.04, but I believe it runs on any other linux environment.

```
$ cd server
$ ./FastEventServer\_linux\_64bit <path/to/your/service.cfg>
```

### 2. Running on a Mac

```
$ cd server
$ ./FastEventServer\_darwin\_64bit <path/to/your/service.cfg>
```

### 3. Running on a Windows PC

```
$ cd server
$ FastEventServer\_windows\_64bit.exe <path/to/your/service.cfg>
```

## Building

You may want to build the binary yourself e.g. in case you work on a 32-bit environment, or you want to customize the code.

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

The `feconnect` library is available for calling the service API from Python: visit the [GitLab repository webpage](https://gitlab.com/fastevent/code/feconnect-python).

### 2. C++-based driver profiling

You can profile the read-write latency for your driver using the `profile_direct` binary. It produces a CSV file of sent/received timestamps, using the nanosecond-level system clock.

The naming convention is the same as in the case of `FastEventServer`, and the general calling convention would be as follows:

```bash
./profile_direct\_<env>\_<bitwidth> -n 20000 <path/to/your/service.cfg> >`date "+prof_%Y-%m-%d-%H%M%S.csv"` # writing to a time-stamped file
```

You can specify the number of test transactions by the `-n` option (defaults to 10000, if you omit it).

## Adding your own driver

In case you implement your own driver, below are some tips.
I would appreciate it very much if you create a pull request!

### 1. Implementation

Any driver must implement the `fastevent::OutputDriver` interface (declared in `include/driver.h`), which requires any subclass to  have the following members:

1. `static std::string identifier()`: used to specify the driver, just like `uno` or `leonardo`.
2. `static ks::Result<fastevent::OutputDriver> setup(fastevent::Config& cfg)`: used to create an instance of the driver, including any necessary initialization procedures. `cfg` contains the JSON configuration parsed from `service.cfg`.
3. `void update(const char& out)`: update the status of the output based on the `out` byte.
4. `void shutdown()`: the hook for finalizing your driver instance.
   **IMPORTANT NOTE**: because of the current implementation (and because of the nature of the UDP communication), this method may not be always called. Please do not count so much on this method to be called.

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

