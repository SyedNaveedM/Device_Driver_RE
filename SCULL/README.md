# SCULL

An implementation of **scull** (Simple Character Utility for Loading
Localities) as developed in Chapter 3, "Char Drivers", of *Linux
Device Drivers, 3rd Edition* (Rubini, Corbet, Kroah-Hartman).

scull acts on a region of kernel memory as though it were a device.
It has no real hardware behind it, which makes it a portable,
architecture-independent way to study the char driver interface:
device number allocation, `file_operations`, `cdev` registration,
`open`/`release`, and `read`/`write`/`llseek` with `copy_to_user`/
`copy_from_user`.

This covers the basic personality described in the chapter:

- **scull0 - scull3**: four devices, each a memory area that is
  *global* (shared by every file descriptor that opens it) and
  *persistent* (data survives close/reopen, until overwritten or the
  module is unloaded).

The other personalities mentioned in the chapter (scullpipe,
scullsingle, scullpriv, sculluid, scullwuid) belong to Chapter 6 and
are not implemented here.

## Design

Each device's data is stored as a linked list of "quantum sets"
(`struct scull_qset`), where each set holds an array ("qset") of
pointers to fixed-size memory blocks ("quanta"). This avoids ever
needing one huge contiguous `kmalloc`, and lets the device grow (or
shrink, on truncation) one quantum at a time.

Defaults: quantum = 4000 bytes, qset = 1000 entries. Both can be
overridden at module load time.

## Files

- `scull.h` — shared declarations and configurable defaults
- `main.c` — module init/cleanup, device registration, and the
  `open`, `release`, `read`, `write`, `llseek` methods
- `Makefile` — kbuild wrapper (`make`, `make clean`, `make load`,
  `make unload`)
- `scull_load` / `scull_unload` — load the module, read the assigned
  dynamic major number from `/proc/devices`, and create/remove the
  `/dev/scull0`..`/dev/scull3` nodes

## Building and loading

```sh
make
sudo ./scull_load
```

This creates `/dev/scull0` through `/dev/scull3`. Because the major
number is allocated dynamically by default, `scull_load` looks it up
in `/proc/devices` after `insmod` and uses `mknod` to create the
device files.

## Trying it out

```sh
echo "hello scull" > /dev/scull0
cat /dev/scull0
cp /dev/scull0 /tmp/copy
dd if=/dev/zero of=/dev/scull0 bs=1M count=50   # eat some RAM, watch `free`
```

Writing to a device opened `O_WRONLY` truncates it first, just like
a regular file — this is what `scull_open` does via `scull_trim`.

## Unloading

```sh
sudo ./scull_unload
```

## Module parameters

All configurable at `insmod`/`modprobe` time:

| Parameter       | Meaning                              | Default |
|-----------------|---------------------------------------|---------|
| `scull_major`   | major number (0 = dynamic)            | 0       |
| `scull_minor`   | first minor number                    | 0       |
| `scull_nr_devs` | number of scull devices               | 4       |
| `scull_quantum` | bytes per quantum                     | 4000    |
| `scull_qset`    | quanta per quantum set                | 1000    |

Example:

```sh
sudo insmod ./scull.ko scull_nr_devs=2 scull_quantum=8000
```
