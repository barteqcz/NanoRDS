# MicroRDS

MicroRDS is a lightweight, software RDS encoder for Linux.

### Features

- low-resource
- supports PS, PTY, PI, RT, RT+, CT, PTYN
- supports fifo control, so bash scripts to control RDS can be written

### Compilation

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

MicroRDS is a fork from [MiniRDS](https://github.com/Anthony96922/MiniRDS) created by [Anthony96922](https://github.com/Anthony96922), and I would like to say many thanks for the great job, to the author!
