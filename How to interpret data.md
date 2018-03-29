# How to interpret latency data

Mar 11, 2018 -- Keisuke Sehara

Our Leonardo driver sends the command to Arduino only if it changes its current
state (i.e. whether EVENT is on, and whether SYNC is on).
Considering that there are the ON and OFF commands for each of EVENT and SYNC,
the probability that a command is sent to Arduino is 0.5 (per command from the
client).
Therefore, the expected rate for Leonardo driver to send a command to Arduino
is one per $1/0.5 = 2$ commands from the client.

On the other hand, Arduino sends its state history back to the Leonardo driver
every 4 commands.
This means that the rate that Arduino sends the response back to Leonardo driver
is one per $4 \times 2 = 8$ commands from the client.

Now let's consider how long it takes for Leonardo driver to perform the above set
of transactions.
Leonardo driver sends 4 command bytes from Arduino, and Arduino sends 1 byte back
to the driver.
Let us assume that both serial Read and Write takes the same duration, $\delta$,
per byte.
Then, this series of transactions (4 write, 1 read) takes $5T$ time period,
during which 8 commands from the client are taken care of.

Therefore, we can say that the duration required for the driver to take care of
one client command, $\tau$, is expected to be $\tau = \frac{5}{8}\delta$.
Conversely, the (maximal) rate of client command handling, $f$, can be written
as $f = \frac{8}{5\delta}$.

Now we have the average transaction latency $T$ between Leonardo driver and Arduino.
This measures the latency of every 4th writing and the reading from Arduino,
which should correspond to $2\tau$ in the discussion above: $T = 2\delta$,
i.e. $\delta = T/2$.

Therefore:

+ Per-command latency: $\tau = \frac{5}{8}\delta = \frac{5}{16}T$
+ Maximum command rate: $f = \frac{8}{5\delta} = \frac{16}{5T}$

Below is the rough result of the benchmark in my environment (MacOS X):

|Arduino        |Read-Write $T$ (us) |Per-command $\tau$ (us) |Max rate $f$ (kHz) |
|---------------|--------------------|------------------------|-------------------|
|Uno/Atmega16u2 |101                 |31.6                    |31.7               |
|Micro          |545                 |170                     |5.87               |
|Uno            |~3000*              |938                     |1.07               |

\*Note that data rate on Arduino Uno can vary depending on the rate of commands.

