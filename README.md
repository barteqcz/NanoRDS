# MicroRDS

MicroRDS is a lightweight, software RDS encoder for Linux.

![MicroRDS](https://i.imgur.com/8zIlRs4.jpeg)

### Key Features:

- Lightweightness: Designed with efficiency in mind, the system boasts minimal resource requirements, ensuring optimal performance on a variety of hardware configurations.

- Extensive protocol support: Built-in compatibility for PS, PTY, PI, RT, RT+, CT, PTYN, ECC, LIC, AF.

- Seamless script usage: Flexibility of using familiar scripting languages like Bash, Python, and more, to effortlessly control RDS through FIFO.

### Plugins

- [MicroRDS_SSH](https://github.com/barteqcz/MicroRDS_SSH) - This is a script in Python that connects to a remote SSH server, runs MicroRDS and controls it with a text file located on the client.

### Documentation

MicroRDS has its own website that contains a very extensive support and documentation. You can find it [here](https://barteqcz.github.io/MicroRDS)

## Installation

### Dependencies
on Debian-based distros, run `sudo apt install libao-dev libsamplerate0-dev` <br>
on Arch-based distros, run `sudo pacman -S libao libsamplerate` <br>
on Fedora, run `sudo dnf install libao-devel libsamplerate-devel` <br>

### Downloading the code

```
git clone https://github.com/barteqcz/MicroRDS
```

### Tweaking the features

If you prefer the program to utilize RBDS, refer to the Makefile file for relevant instructions. RBDS is disabled by default.

### Compilation

Go to the `src` folder, and simply run `make`. If you want to enable RBDS, do it before running `make`.

## Usage

Once you compiled the program, use `./micrords` to run it. 

## Troubleshooting

The output volume of your subcarriers depends also on the OS output volume. I compared it to other radio stations. On my hardware, it seems that setting the OS output volume to 50% is the 'zero-point' for transmitting RDS - 4.5% is the same level as many radio stations' RDS subcarriers.

## Credits

MicroRDS is a fork of [MiniRDS](https://github.com/Anthony96922/MiniRDS) written by [Anthony96922](https://github.com/Anthony96922)
