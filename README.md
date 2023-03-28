# MicroRDS

MicroRDS is a lightweight, software RDS encoder for Linux.

![MicroRDS](/doc/MicroRDS.jpg)

### Features

- low-resource requirements
- supports PS, PTY, PI, RT, RT+, CT, PTYN
- supports FIFO control, so scripts in Bash, Python, etc. to control RDS can be written.

## Installation

### Dependencies
on Debian-based & Ubuntu-based distros, run `sudo apt install libao-dev libsamplerate0-dev` <br>
on Arch-based distros, run `sudo pacman -S libao libsamplerate` <br>
on Fedora, run `sudo dnf install libao libsamplerate` <br>

### Downloading the code

```
git clone https://github.com/barteqcz/MicroRDS.git
```
### Tweaking the features

In case you want the program to transmit pilot, and broadcast Stereo, or you want it to use RBDS, take a look into the *Makefile* file. There you will find descriptions.<br>
Stereo encoder is enabled, but RDBS is disabled by default.

### Compilation

Go to the `src` folder, and simply run `make`

## Usage

Once you compiled the program, use `./micrords` to run it. 

To see available FIFO commands list, see [FIFO command list](https://github.com/barteqcz/MicroRDS/blob/main/doc/fifo_command_list.md)

To see available program-built-in commands, run `./micrords --help`
## Credits

MicroRDS is a fork of [MiniRDS](https://github.com/Anthony96922/MiniRDS) created by [Anthony96922](https://github.com/Anthony96922), and I would like to thank the author for the wonderful job!
