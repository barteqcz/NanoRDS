### MicroRDS FIFO command list

This is a complete list of commands MicroRDS handles via FIFO.

Create the control pipe and enable FIFO control:
```
mkfifo rds
./micrords --ctl rds
```
Then you can send the commands to change particular elements of the RDS.

Every line must start with a valid command, followed by one space character, and the desired value. Any other line format is silently ignored. 

For example `TA ON` switches the Traffic Announcement flag to *on*, and any other value switches it to *off*.

## Commands

### `AF`
Sets AF frequency. It uses two flags:
#### AF a - to add a frequency to the AF list.

Example usage: `AF a 87.7`

#### AF r - to reset the AF list and delete all frequencies.

Example usage: `AF r`

### `PI`
Sets the PI code. This takes 4 hexadecimal digits.

`PI 1000`

### `PS`
The Program Service text. Maximum is 8 characters. This is usually static, such as the station's callsign, but can be dynamically updated.

`PS Hello`

### `RT`
The Radiotext to be displayed. This can be up to 64 characters.

`RT This is a Radiotext message`

### `TA`
To signal to receivers that there is traffic information currently being broadcast.

`TA 1`

### `TP`
To signal to receivers that the broadcast can carry traffic info.

`TP 1`

### `MS`
The Music/Speech flag. Music is 1 and speech is 0.

`MS 1`

### `DI`
Decoder Identification. A 4-bit decimal number. Usually only the "stereo" flag (1) is set.

`DI 1`

### `PTY`
Set the Program Type (full list [here](https://github.com/barteqcz/MicroRDS/blob/main/doc/pty.md)) Used to identify the format the station is broadcasting. Valid range is 0-31. Each code corresponds to a Program Type text.

`PTY 0`

### `MPX`
Set volumes in percent modulation for individual MPX subcarrier signals. The first value is responsible for stereo pilot, but the second - for RDS strength.

`MPX 9,9`

### `VOL`
Set the output volume in percent.

`VOL 100`

### `PTYN`
Program Type Name. Used for broadcasting a more specific format identifier. `PTYN OFF` disables broadcasting the PTYN.

`PTYN CHR`

### `RTP`
Radiotext Plus tagging data. Comma-separated values specifying content type, start offset and length. <br> Format: `<content type 1>,<start 1>,<length 1>,<content type 2>,<start 2>,<length 2>`.

`RTP 0,0,0,0,0,0`

### `RTPF`
Sets the Radiotext Plus "Running" and "Toggle" flags.

`RTPF 1,0`
