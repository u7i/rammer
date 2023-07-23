#define _GNU_SOURCE
#define RAMFS_BLOCK_SIZE 4096
#define ROOT "/tmp/ramdisks"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

_Noreturn void panic(bool machine_readable, const char* emitter_name, const char* detail)
{
    machine_readable ?
        fprintf(stderr, "%s, %d\n", emitter_name, errno) :
        fprintf(stderr, "[Failed] %s (%s)\n", detail, strerror(errno));

    exit(EXIT_FAILURE);
}

int allocate_ramdisk(size_t ref_size, const char* mount_point, __uid_t uid, __gid_t gid, __mode_t mode, size_t* real_size)
{
    // Round up the size to the closes multiple of block-size
    size_t size = ref_size % RAMFS_BLOCK_SIZE ? RAMFS_BLOCK_SIZE * (1 + ref_size / RAMFS_BLOCK_SIZE) : ref_size;
    real_size && (*real_size = size);

    // Create a tmpfs partition
    char* options;
    asprintf(&options, "size=%zu,uid=%d,gid=%d,mode=%o", size, uid, gid, mode);

    return mount("tmpfs", mount_point, "tmpfs", 0, options);
}

int destroy_ramdisk(const char* mount_point)
{
    return -(umount(mount_point) == -1 || remove(mount_point) == -1);
}

int lock_directory(const char* path, int mode)
{
    mkdir(path, mode);
    int fd = open(path, O_RDONLY);

    if (fd == -1) return -1;
    if (flock(fd, LOCK_EX) == -1) return -1;

    return fd;
}

int lock_file(const char* path, int mode)
{
    int fd = open(path, O_RDWR | O_CREAT, mode);

    if (fd == -1) return -1;
    if (flock(fd, LOCK_EX) == -1) return -1;

    return fd;
}

void unlock_entry(int fd)
{
    flock(fd, LOCK_UN);
    close(fd);
}

int init_root_folder()
{
    // Check if lock file exists
    struct stat st = { 0 };
    const char* mount_trait = ROOT"/.rammer";

    if (stat(mount_trait, &st) != -1)
    {
        return 0;
    }

    // Acquire the lock on the whole mount point
    int lock = lock_directory(ROOT, 0777);
    if (lock == -1) return -1;

    // Try to mount the ramdisk
    int res = allocate_ramdisk(10 * 1024 * 1024, ROOT, 0, 0, 0777, NULL);
    (errno == EBUSY) && (res = 0);

    // Create the trait file
    int lfd = open(mount_trait, O_WRONLY | O_CREAT, 0700);
    (lfd == -1) && (res = -1);
    close(lfd);

    unlock_entry(lock);
    return res;
}

ssize_t reserve_id()
{
    // Lock the counter
    int fd = lock_file(ROOT"/.next-id", 0700);
    if (fd == -1) return -1;

    // Update the id in file
    FILE* file = fdopen(fd, "r+");
    if (!file) return -1;

    // Read the current id itself
    ssize_t w = 0;
    fscanf(file, "%zd\n", &w);

    // Update the file content
    ftruncate(fd, 0);
    rewind(file);
    fprintf(file, "%zd\n", w + 1);

    fclose(file);
    unlock_entry(fd);
    return w;
}

void print_help()
{
    printf(
        "Rammer 0.1.\n"
        "A fixed size ramdisk creation tool.\n\n"
        "Usage:\n"
        " rammer [-m] alloc reference-size-in-bytes\n"
        " rammer [-m] free fid\n"
        " rammer -h\n\n"
        "Options:\n"
        " -h Display this help\n"
        " -m Emit machine-readable output ( for usage in scripts )\n"
    );
}

int main(int argc, char** argv)
{
    bool machine_readable = false;

    // Save permissions with which the rammer was called
    __uid_t user_uid = getuid();
    __gid_t user_gid = getgid();
    setuid(0); setgid(0);

    // Short args
    int option;
    while ((option = getopt(argc, argv, "hm")) != -1)
    {
        if (option == 'h')
        {
            print_help();
            exit(EXIT_SUCCESS);
        }

        if (option == 'm')
        {
            machine_readable = true;
        }
    }

    // Positional arguments
    if (argc - optind < 2)
    {
        errno = EINVAL;
        panic(machine_readable, "input", "Not enough arguments were given");
    }

    // Process commands
    if (strcmp(argv[optind], "alloc") == 0)
    {
        size_t ref_size = strtoul(argv[optind + 1], NULL, 10);

        // Init the root folder where ramdisks mount points will be placed
        if (init_root_folder() == -1)
        {
            panic(machine_readable, "init-root-folder", "Can't init the rammer root directory");
        }

        // Get the id of a new mountpoint
        ssize_t id = reserve_id();
        if (id == -1)
        {
            panic(machine_readable, "reserve-id", "Can't generate an id for the ramdisk");
        }

        // Create the mount point directory
        char* mp;
        asprintf(&mp, ROOT"/%zu", id);
        if (mkdir(mp, 0700) == -1)
        {
            panic(machine_readable, "create-endpoint", "Can't create the ramdisk mount point");
        }

        size_t real_size;
        if (allocate_ramdisk(ref_size, mp, user_uid, user_gid, 0770, &real_size) == -1)
        {
            panic(machine_readable, "allocate-ramdisk", "Can't allocate the ramdisk");
        }

        machine_readable ?
            printf("%zu, %s, %zu\n", id, mp, real_size) :
            printf("[Ok] %s ( allocated %zu bytes, id %zu )\n", mp, real_size, id);

        free(mp);
    }
    else if (strcmp(argv[optind], "free") == 0)
    {
        char* mp;
        asprintf(&mp, ROOT"/%s", argv[optind + 1]);

        if (destroy_ramdisk(mp) == -1)
        {
            free(mp);
            panic(machine_readable, "destroy-ramdisk", "Can't destroy a ramdisk");
        }

        machine_readable ?
            printf("\n") :
            printf("[Ok] Destroyed ramdisk %s\n", argv[optind + 1]);

        free(mp);
    }

    return EXIT_SUCCESS;
}