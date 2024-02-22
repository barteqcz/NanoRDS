## FIFO creation
Create the control pipe in the same folder MicroRDS executable file is located in:
```
mkfifo rds
```
and run MicroRDS with the appropriate flag:
```
./micrords --ctl rds
```
Now you can send commands to fifo, to control RDS (manually, or using some automated script).

Every line must start with a valid command, <b>followed by one space character</b>, and the desired value. Any other line format is silently ignored. 

&nbsp;

## FIFO Commands

`PS` - PS stands for Program Service name and it allows to broadcast a 'title' for the station that is displayed on every receiver with RDS support (that's the most basic feature). Its length can be up to 8 characters. This is how you can set it in MicroRDS via fifo
```
PS * RDS *
```
By default, MicroRDS centers the PS name. So, for example, if you enter
`PS TEST`
the displayed PS won't be `TEST____`, but `__TEST__`. If you don't want that, just fill the remaining space with spaces
`PS TEST    `

&nbsp;

`RT` - stands for RadioText and is basically a longer message that is displayed in most RDS receivers. It can contain up to 64 characters, and here's how you can set it
```
RT This is a RadioText message :)
```

&nbsp;

`RTPF` - a 2-bit integer, applicable when you want to transmit RT+. It sets RT+ flags - 'running' and 'toggle'

- RTPF 0 - unsets both flags,

- RTPF 1 - sets the toggle flag,

- RTPF 2 - sets the running flag,

- RTPF 3 - sets both flags
```
RTPF 2
```

&nbsp;

`RTP` - provides data for the RT+ group (in MicroRDS, RT+ is transmitted in 11A group). Its format is

`<content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>`. 

It takes data from the RT
```
RTP 4,0,6,1,9,8
```

&nbsp;

`PTY` - defines Program Type, so basically what content does the station broadcast. In MicroRDS, PTY is set using a code. [Full list of PTY codes](pty.md)
```
PTY 10
```

&nbsp;

`PTYN` - basically a PTY extension. Able to hold up to 8 characters, typically used to add some additional info about the broadcast content type.
```
PTYN CHR
```

&nbsp;

`PI` - Program Identification is a unique 16-bit hexadecimal, 4-character code that identifies the station. Here's how you can set it in MicroRDS
```
PI 2F08
```

&nbsp;

`AF` - AF stands for Alternative Frequencies and allows the receiver to switch between pointed frequencies when the signal weakens. AF has two methods - both are supported by MicroRDS. 

This is how you can use AF method A in MicroRDS. It is recommended that the first frequency on the list is the one your current transmitter is on
```
AF s 101.8 87.7 96.4
```
This is how you can set AF method B. In method B, you create frequency pairs. So as in the given example, 87.7 will be the main frequency and it will define paired 87.7 with 96.4
```
AF s 87.7 96.4 87.7
```
If you wish to clear the AF list, use
```
AF c
```

&nbsp;

`ECC` - 8-bit hexadecimal code that defines country of origin of the station. It can be the same for many countries, since the second condition for country identification is the PI code. If you want to disable it, set its value to 0
```
ECC E2
```

&nbsp;

`LIC` - 8-bit hexadecimal code - its value defines the language of the station. If you want to disable it, set its value to 0
```
LIC 06
```

&nbsp;

`MS` - music/speech flag. Music is 1 and speech is 0.
```
MS 1
```

&nbsp;

`TP` - Traffic Program flag. Defines whether station broadcasts traffic info. If yes, it's set to 1; if no, it's set to 0
```
TP 1
```

&nbsp;

`TA` - Traffic Announcement flag - signals that there are traffic announcements being broadcasted at the moment
```
TA 1
```

&nbsp;

`DI` - Decoder Information - identifies different operating modes. This enables controlling of individual decoders. Additionally it indicates if static or dynamic PTY codes are transmitted.
```
DI 1
```

&nbsp;

`MPX` - controls subcarriers volume. The first number is Stereo pilot strength, the second one controls RDS carrier strength. For example, this will set Stereo pilot level to 9%, and RDS subcarrier level to 4.5%
```
MPX 9,4.5
```
If you want to disable Stereo pilot or RDS carrier, simply set its value to 0. For example this, will turn Stereo pilot off
```
MPX 0,9
```

&nbsp;

`RESET` - doesn't take any arguments - resets all the encoder parameters to the default ones
```
RESET
```
