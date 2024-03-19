# Linux trace tool


## Introduction

This python tool helps to analysis media stack trace logs combining ftrace events from libva, media driver and Linux kernel mode driver (e.g. i915).


## Linux trace capture

1. Install trace-cmd:

        sudo apt-get install trace-cmd

2. Grant write access to trace node for application:

        sudo chmod 777 /sys/kernel/debug/
        sudo chmod 777 /sys/kernel/debug/tracing/
        sudo chmod 777 /sys/kernel/debug/tracing/trace_marker_raw

3. Enable libva trace:

        export LIBVA_TRACE = FTRACE

    to enable libva buffer data capture

        export LIBVA_TRACE_BUFDATA = 1

4. Run application under trace-cmd in a proxy mode:

        trace-cmd record -e i915 <workflow-cmd-line>

5. Output is "trace.dat"

Alternatively you can collect trace data in separate terminal.
It is useful if you want to profile daemon or a service:

1. Start trace capture:

        sudo trace-cmd record -e i915

2. Run test app in another terminal
3. Stop capturing in the first terminal
4. Output is "trace.dat"


## Trace post-processing and analysis

    python3 main.py [-raw] file.dat|file.etl [file.dat|file.etl ...]

Options:

* `-raw` - Parse trace events and dump into <trace-file>.csv file.

Output:

* `<trace-file>.json.gz` - visualized trace activity, open in `<chrome://tracing/>` or `<edge://tracing/>`
* `<trace-file>_stat.csv` - statistic of trace activity, open in Excel
* `<trace-file>_surface.csv` - surface attributes and runtime usage in GPU, open in Excel
* `<trace-file>_rtlog.txt` - iHD driver runtime log


## Trace tool manifests

Trace tool uses manifests to decode trace data. Each trace module available in trace tool
needs to have a manifest file in manifests folder. The manifest file could be either in
MSFT ETW XML manifest format or in json format. Current supported traces:

* [libva_trace.man](./manifests/libva_trace.man) - libva trace manifest in MSFT ETW XML
* [Intel-Media-Open.json](./manifests/Intel-Media-Open.json) - iHD media driver trace manifest in json

## Trace Modules

Trace tool loads trace modules from the [modules](./modules) folder. Two types of modules
are supported:

* Trace readers
* Trace handlers

Readers support reading data from the trace without performing any action on the read data.
Handlers perform actions over read data. Effectively trace readers provide input to trace
handlers.

Trace tool loads modules by class names.

### Trace core

Trace core loads trace modules making them available for trace tool. Key interfaces:

| Interface | Description |
| --------- | ----------- |
| `core.regParser(id, parser) -> int` | Registers trace header `parser` to the core. `id` is 4bytes trace identifier. |
| `core.regHandler(sys, name, handler) -> None` | Registers event handler. Set name to None for common trace handler. |
| `core.getContext() -> dict` | Get share context from the core. |

### Trace readers

Trace reader module is responsible for parsing trace file into trace events and call
trace handlers one by one in event timestamp order.

Trace reader is driven by trace core. The following interfaces are required to be
provide by the trace reader module implentation:

| Interface | Description |
| --------- | ----------- |
| `open(self, file, options) -> int` | Open trace file, returns 0 for sucess, < 0 for failures. User command line options are provided in args. |
| `setParser(self, parsers) -> None` | Set trace header parsers. Since all Linux user space traces share single trace_marker_raw entry, each user trace(libva and iHD) need register its own header parser to identify itself. |
| `syncSource(self, src) -> int` | For sync timestamp across multi trace readers. Return -1 for no support. |
| `process(self, filter, handler) -> None` | Starts trace event process with filter and handler callbacks. Filter callback could speed up event process. |

Currently supported trace reader modules:

| Module     | Description |
| ---------- | ----------- |
| ftrace.py  | Linux ftace file reader, trace file from trace-cmd |

### Trace handlers

Trace event handler module customizes events handling. Since all handler modules are
seperate instances, trace core provides shared context to share data between modules.

By default, shared context provides the following:

| Name in context | Description |
| --------------- | ----------- |
| `UI`            | Instance of class `writeUI` (see [writeUI.py](./writeUI.py). Class writes events for chrome://tracing. |
| `Stack`         | Instance of class `callStack` (see [callStack.py](./callStack.py). Class provides call stack of trace event. Call stack is built from event sequence from the same process id and thread id. |
| `Stat`          | Instance of class `statistic` (see [statistic.py](./statistic.py). Class provides statistics for specific events. |
| `Output`        | Output path string in case module needs to create its own output file. |

Handler module only interact with core, should not export interface to external directly. Module register its own event handlers to core through `core.regHandler(sys, name, handler)`.

It is possible that multi modules register their own handlers for the same event. Core will call these callbacks one by one when target event occurs.

Handler module could write output into `UI` or `Stat` in share context, also could create and write its own output file and format. Output file path is available in share context `Output`.

In case a handler module is targeted to provide a common service, it could export its name and instance in share context. Its name in share context should be unique, other module use this name to get service instance. The service interface is defined by module itself. see example [surface.py](./modules/surface.py).

To add new trace support, handler module for this new trace MUST register event header parser to core, through `core.regParser(id, parser)`. This event header parser is to detect and parse trace header, otherwise trace reader could not recognize this new trace. The id for this new trace should be unique. refer example [libva.py](./modules/libva.py).

Currently supported handler modules:
| Module     | Description |
| ---------- | ----------- |
| i915.py    | i915 trace handler to extract GPU workload submit & execution timing |
| libva.py   | libva trace handler |
| iHD.py     | Intel iHD open source media driver trace handler |
| surface.py | Handler tracks surface object & attributes across iHD and i915 traces |

## Making changes in the tool

Make sure to run unit tests before creating PR:

    cd tracetool
    python3 -m unittest

Make sure trace event and event data are backward compatible.

