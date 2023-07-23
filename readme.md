![Logo](./logo.svg)

Rammer is a tool for creating ramdisks without any permissions!

### Installation 
```bash
git clone https://github.com/u7i/rammer
mkdir build && cmake ../ 
make -j4 && sudo make install
```

### Example
```bash
rammer alloc 4096
rammer free 0
```

### Usage
```
rammer [-mh] alloc {reference-size}
- Allocates a new ramdisk with size of AT LEAST {reference-size} bytes.
  {reference-size} will be rounded up to closest multiple of 4096 ( tmpfs block size ).
    
  In case of success creates a new ramdisk mounted to /tmp/ramdisks/{id}.  
  Note, that all allocated disks will be destroyed on system reboot.
  
- Machine-Readable Output: 
  Ok      -> id, mount-point, real-size [ ExitCode = 0 ]
  Failure -> failed-module-name, errno  [ ExitCode = 1 ]
```

```
rammer [-mh] free {id}
- Destroys previously allocated ramdisk.

- Machine-Readable Output: 
  Ok      -> #Nothing                  [ ExitCode = 0 ]
  Failure -> failed-module-name, errno [ ExitCode = 1 ]
```

```
rammer -m ... -> Formats output in the machine readable form
rammer -h ... -> Prints a help
```
