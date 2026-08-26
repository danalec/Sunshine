// set-accel-flat — define o perfil de aceleração FLAT (1:1) num device libinput
// por nome. Fica rodando para reaplicar quando o device reaparecer.
// uso: set-accel-flat [nome-do-device]   (default: "libvirtualhid Mouse")
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int
open_restricted(const char *path, int flags, void *userdata) {
    (void)userdata;
    return open(path, flags);
}

static void
close_restricted(int fd, void *userdata) {
    (void)userdata;
    close(fd);
}

static const struct libinput_interface iface = {
    open_restricted,
    close_restricted,
};

int
main(int argc, char **argv) {
    const char *target = argc > 1 ? argv[1] : "libvirtualhid Mouse";

    struct udev *udev = udev_new();
    if (!udev) {
        fprintf(stderr, "udev_new falhou\n");
        return 1;
    }
    struct libinput *li = libinput_udev_create_context(&iface, NULL, udev);
    if (!li) {
        fprintf(stderr, "libinput_udev_create_context falhou\n");
        return 1;
    }
    if (libinput_udev_assign_seat(li, "seat0") != 0) {
        fprintf(stderr, "assign_seat falhou\n");
        return 1;
    }

    fprintf(stderr, "set-accel-flat: vigiando device \"%s\" (ctrl-c para sair)\n", target);

    while (1) {
        libinput_dispatch(li);
        struct libinput_event *ev;
        while ((ev = libinput_get_event(li))) {
            enum libinput_event_type type = libinput_event_get_type(ev);
            if (type == LIBINPUT_EVENT_DEVICE_ADDED) {
                struct libinput_device *dev = libinput_event_get_device(ev);
                if (strcmp(libinput_device_get_name(dev), target) == 0 &&
                    libinput_device_config_accel_is_available(dev)) {
                    enum libinput_config_status st =
                        libinput_device_config_accel_set_profile(dev, LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT);
                    fprintf(stderr, "device \"%s\" (%s): accel FLAT %s\n", target,
                            libinput_device_get_sysname(dev),
                            st == LIBINPUT_CONFIG_STATUS_SUCCESS ? "OK" : "FALHOU");
                }
            }
            libinput_event_destroy(ev);
        }
        usleep(200000);
    }
}
