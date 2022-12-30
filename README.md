# MicroRDS

MicroRDS is a lightweight, software RDS encoder for Linux.

### Features

- low-resource requirements
- supports PS, PTY, PI, RT, RT+, CT, PTYN
- supports fifo control, so bash scripts to control RDS can be written

### Installing dependencies & compilation

on Debian-based & Ubuntu-based distros, run `sudo apt install libao-dev libsamplerate0-dev` <br>
on Arch-based distros, run `sudo pacman -S libao libsamplerate` <br>
on Fedora, run `sudo dnf install libao libsamplerate` <br>

and after installing the dependencies, run the following to compile the program:

```
git clone https://github.com/barteqcz/MicroRDS
cd MicroRDS
cd src
make
```

### Usage

Once you compiled the program, use `./micrords` to run it. 

To see available commands list, see [command list](https://github.com/barteqcz/MicroRDS/blob/main/doc/command_list.md)

### Credits

MicroRDS is a fork of [MiniRDS](https://github.com/Anthony96922/MiniRDS) created by [Anthony96922](https://github.com/Anthony96922), and I would like to say many thanks for the great job, to the author!
