#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/usb/input.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define DRIVER_NAME    "keyboard_tablet"
#define DRIVER_AUTHOR  "Alexander Stepanov"
#define DRIVER_DESC    "Simulate table like a keyboard."
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define ID_VENDOR_TABLET  0x056a /* Wacom Co. */
#define ID_PRODUCT_TABLET 0x0301 /* Ltd CTL-671 */

#define USB_PACKET_LEN  10
#define WHEEL_THRESHOLD 4

#define MAX_X 1920
#define MAX_Y 1080

#define MAX_VALUE 0x7F

#define X_FACTOR (MAX_X / MAX_VALUE + 1)
#define Y_FACTOR (MAX_Y / MAX_VALUE + 1)

struct tablet {
    unsigned char     *data;
    dma_addr_t         data_dma;
    struct input_dev  *input_dev;
    struct usb_device *usb_dev;
    struct urb        *irq;
    int                old_wheel_pos;
    char               phys[32];
};

typedef struct tablet tablet_t;

struct container_urb {
    struct urb *urb;
    struct work_struct work;
};

typedef struct container_urb container_urb_t;

static bool pen_enter;
static int pressed_key;

static struct workqueue_struct *workq;

static struct input_dev *keyboard;

static void press_key(u16 x, u16 y) {
    if (y <= 9) {         /* first row */
        if (x <= 5) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "esc");
            pressed_key = KEY_ESC;
        } else if (x <= 11) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "1");
            pressed_key = KEY_1;
        } else if (x <= 17) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "2");
            pressed_key = KEY_2;
        } else if (x <= 23) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "3");
            pressed_key = KEY_3;
        } else if (x <= 29) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "4");
            pressed_key = KEY_4;
        } else if (x <= 35) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "5");
            pressed_key = KEY_5;
        } else if (x <= 41) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "6");
            pressed_key = KEY_6;
        } else if (x <= 47) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "7");
            pressed_key = KEY_7;
        } else if (x <= 53) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "8");
            pressed_key = KEY_8;
        } else if (x <= 59) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "9");
            pressed_key = KEY_9;
        } else if (x <= 65) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "0");
            pressed_key = KEY_0;
        } else if (x <= 71) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "-");
            pressed_key = KEY_MINUS;
        } else if (x <= 77) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "=");
            pressed_key = KEY_EQUAL;
        } else if (x <= 83) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "backspace");
            pressed_key = KEY_BACKSPACE;
        }
    } else if (y <= 19) { /* second row */
        if (x <= 5) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "tab");
            pressed_key = KEY_TAB;
        } else if (x <= 11) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "q");
            pressed_key = KEY_Q;
        } else if (x <= 17) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "w");
            pressed_key = KEY_W;
        } else if (x <= 23) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "e");
            pressed_key = KEY_E;
        } else if (x <= 29) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "r");
            pressed_key = KEY_R;
        } else if (x <= 35) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "t");
            pressed_key = KEY_T;
        } else if (x <= 41) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "y");
            pressed_key = KEY_Y;
        } else if (x <= 47) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "u");
            pressed_key = KEY_U;
        } else if (x <= 53) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "i");
            pressed_key = KEY_I;
        } else if (x <= 59) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "o");
            pressed_key = KEY_O;
        } else if (x <= 65) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "p");
            pressed_key = KEY_P;
        } else if (x <= 71) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "[");
            pressed_key = KEY_LEFTBRACE;
        } else if (x <= 77) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "]");
            pressed_key = KEY_RIGHTBRACE;
        } else if (x <= 83) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "\\");
            pressed_key = KEY_BACKSLASH;
        }
    } else if (y <= 29) { /* third row */
        if (x <= 5) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "caps");
            pressed_key = KEY_CAPSLOCK;
        } else if (x <= 11) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "a");
            pressed_key = KEY_A;
        } else if (x <= 17) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "s");
            pressed_key = KEY_S;
        } else if (x <= 23) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "d");
            pressed_key = KEY_D;
        } else if (x <= 29) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "f");
            pressed_key = KEY_F;
        } else if (x <= 35) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "g");
            pressed_key = KEY_G;
        } else if (x <= 41) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "h");
            pressed_key = KEY_H;
        } else if (x <= 47) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "j");
            pressed_key = KEY_J;
        } else if (x <= 53) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "k");
            pressed_key = KEY_K;
        } else if (x <= 59) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "l");
            pressed_key = KEY_L;
        } else if (x <= 65) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, ";");
            pressed_key = KEY_SEMICOLON;
        } else if (x <= 71) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "'");
            pressed_key = KEY_APOSTROPHE;
        } else if (x <= 83) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "enter");
            pressed_key = KEY_ENTER;
        }
    } else if (y <= 39) {              /* fourth row */
        if (x <= 11) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "left shift");
            pressed_key = KEY_LEFTSHIFT;
        } else if (x <= 17) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "z");
            pressed_key = KEY_Z;
        } else if (x <= 23) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "x");
            pressed_key = KEY_X;
        } else if (x <= 29) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "c");
            pressed_key = KEY_C;
        } else if (x <= 35) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "v");
            pressed_key = KEY_V;
        } else if (x <= 41) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "b");
            pressed_key = KEY_B;
        } else if (x <= 47) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "n");
            pressed_key = KEY_N;
        } else if (x <= 53) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "m");
            pressed_key = KEY_M;
        } else if (x <= 59) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, ",");
            pressed_key = KEY_COMMA;
        } else if (x <= 65) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, ".");
            pressed_key = KEY_DOT;
        } else if (x <= 71) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "/");
            pressed_key = KEY_SLASH;
        } else if (x <= 83) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "right shift");
            pressed_key = KEY_RIGHTSHIFT;
        }
    } else {
        if (x <= 5) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "left control");
            pressed_key = KEY_LEFTCTRL;
        } else if (x <= 11) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "meta");
            pressed_key = KEY_LEFTMETA;
        } else if (x <= 17) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "left alt");
            pressed_key = KEY_LEFTALT;
        } else if (x <= 59) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "space");
            pressed_key = KEY_SPACE;
        } else if (x <= 65) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "right alt");
            pressed_key = KEY_RIGHTALT;
        } else if (x <= 77) {
            if (y <= 45) {
                if (x <= 69) {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "delete");
                    pressed_key = KEY_DELETE;
                } else if (x <= 73) {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "up");
                    pressed_key = KEY_UP;
                } else {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "grave");
                    pressed_key = KEY_GRAVE;
                }
            } else {
                if (x <= 69) {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "left");
                    pressed_key = KEY_LEFT;
                } else if (x <= 73) {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "down");
                    pressed_key = KEY_DOWN;
                } else {
                    printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "right");
                    pressed_key = KEY_RIGHT;
                }
            }
        } else if (x <= 83) {
            printk(KERN_INFO "%s: pressed %s\n", DRIVER_NAME, "right control");
            pressed_key = KEY_RIGHTCTRL;
        }
    }
}

static void down_keyboard(u16 x, u16 y) {
    press_key(x, y);
    input_report_key(keyboard, pressed_key, 1);
    input_sync(keyboard);
}

static void up_keyboard(void) {
    input_report_key(keyboard, pressed_key, 0);
    input_sync(keyboard);
}

static void work_irq(struct work_struct *work) {
    container_urb_t *container = container_of(work, container_urb_t, work);
    struct urb *urb;
    int retval;
    u16 x, y;
    tablet_t *tablet;
    unsigned char *data;

    if (container == NULL) {
        printk(KERN_ERR "%s: %s - container is NULL\n", DRIVER_NAME, __func__);
        return;
    }

    urb = container->urb;
    tablet = urb->context;
    data = tablet->data;

    if (urb->status != 0) {
        printk(KERN_ERR "%s: %s - urb status is %d\n", DRIVER_NAME, __func__, urb->status);
        kfree(container);
        return;
    }

    switch(data[1]) {
        case 0xf1:
            if (!pen_enter) {
                x = data[3] * X_FACTOR;
                y = data[5] * Y_FACTOR;

                down_keyboard(x / 16, y / 9);

                printk(KERN_INFO "%s: pen enters %d %d\n", DRIVER_NAME, x, y);
                pen_enter = true;
            }
            break;
        case 0xf0:
            if (pen_enter) {
                up_keyboard();

                printk(KERN_INFO "%s: pen leaves\n", DRIVER_NAME);
                pen_enter = false;
            }
            break;
        default:
            break;
    }

    retval = usb_submit_urb (urb, GFP_ATOMIC);
    if (retval)
        printk(KERN_ERR "%s: %s - usb_submit_urb failed with result %d\n", DRIVER_NAME, __func__, retval);

    kfree(container);
}

static void tablet_irq(struct urb *urb) {
    container_urb_t *container = kzalloc(sizeof(container_urb_t), GFP_KERNEL);
    container->urb = urb;
    INIT_WORK(&container->work, work_irq);
    queue_work(workq, &container->work);
}

static int tablet_open(struct input_dev *dev) {
    tablet_t *tablet = input_get_drvdata(dev);

    tablet->old_wheel_pos = -WHEEL_THRESHOLD - 1;
    tablet->irq->dev = tablet->usb_dev;
    if (usb_submit_urb(tablet->irq, GFP_KERNEL))
        return -EIO;

    return 0;
}

static void tablet_close(struct input_dev *dev) {
    tablet_t *tablet = input_get_drvdata(dev);
    usb_kill_urb(tablet->irq);
}

static int tablet_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_device *usb_device = interface_to_usbdev(interface);
    tablet_t *tablet;
    struct input_dev *input_dev;
    struct usb_endpoint_descriptor *endpoint;
    int error = -ENOMEM;

    printk(KERN_INFO "%s: probe checking tablet\n", DRIVER_NAME);

    tablet = kzalloc(sizeof(tablet_t), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!tablet || !input_dev) {
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate device\n", DRIVER_NAME);
        return error;
    }

    tablet->data = (unsigned char *)usb_alloc_coherent(usb_device, USB_PACKET_LEN, GFP_KERNEL, &tablet->data_dma);
    if (!tablet->data) {
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate coherent\n", DRIVER_NAME);
        return error;
    }

    tablet->irq = usb_alloc_urb(0, GFP_KERNEL);
    if (!tablet->irq) {
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate urb\n", DRIVER_NAME);
        return error;
    }

    tablet->usb_dev = usb_device;
    tablet->input_dev = input_dev;

    usb_make_path(usb_device, tablet->phys, sizeof(tablet->phys));
    strlcat(tablet->phys, "/input0", sizeof(tablet->phys));

    input_dev->name = DRIVER_NAME;
    input_dev->phys = tablet->phys;
    usb_to_input_id(usb_device, &input_dev->id);
    input_dev->dev.parent = &interface->dev;

    input_set_drvdata(input_dev, tablet);

    input_dev->open = tablet_open;
    input_dev->close = tablet_close;

    endpoint = &interface->cur_altsetting->endpoint[0].desc;

    usb_fill_int_urb(
        tablet->irq, usb_device,
        usb_rcvintpipe(usb_device, endpoint->bEndpointAddress),
        tablet->data, USB_PACKET_LEN,
        tablet_irq, tablet, endpoint->bInterval
    );

    usb_submit_urb(tablet->irq, GFP_ATOMIC);

    tablet->irq->transfer_dma = tablet->data_dma;
    tablet->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    error = input_register_device(tablet->input_dev);
    if (error) {
        usb_free_urb(tablet->irq);
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when register device\n", DRIVER_NAME);
        return error;
    }

    usb_set_intfdata(interface, tablet);

    pen_enter = false;
    printk(KERN_INFO "%s: device is conected\n", DRIVER_NAME);

    return 0;
}

static void tablet_disconnect(struct usb_interface *interface) {
    tablet_t *tablet = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    if (tablet) {
        usb_kill_urb(tablet->irq);
        input_unregister_device(tablet->input_dev);
        usb_free_urb(tablet->irq);
        usb_free_coherent(interface_to_usbdev(interface), USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);

        printk(KERN_INFO "%s: device was disconected\n", DRIVER_NAME);
    }
}

static struct usb_device_id tablet_table [] = {
    { USB_DEVICE(ID_VENDOR_TABLET, ID_PRODUCT_TABLET) },
    { },
};

MODULE_DEVICE_TABLE(usb, tablet_table);

static struct usb_driver tablet_driver = {
    .name       = DRIVER_NAME,
    .probe      = tablet_probe,
    .disconnect = tablet_disconnect,
    .id_table   = tablet_table,
};

static int __init keyboard_tablet_init(void) {
    int result = usb_register(&tablet_driver);

    if (result < 0) {
        printk(KERN_ERR "%s: usb register error\n", DRIVER_NAME);
        return result;
    }

    workq = create_workqueue("workqueue");
    if (workq == NULL) {
        printk(KERN_ERR "%s: allocation workqueue error\n", DRIVER_NAME);
        return -1;
    }

    keyboard = input_allocate_device();
    if (keyboard == NULL) {
        printk(KERN_ERR "%s: allocation device error\n", DRIVER_NAME);
        return -1;
    }

    keyboard->name = "virtual keyboard";

    set_bit(EV_KEY, keyboard->evbit);
    set_bit(KEY_ESC, keyboard->keybit);
    set_bit(KEY_1, keyboard->keybit);
    set_bit(KEY_2, keyboard->keybit);
    set_bit(KEY_3, keyboard->keybit);
    set_bit(KEY_4, keyboard->keybit);
    set_bit(KEY_5, keyboard->keybit);
    set_bit(KEY_6, keyboard->keybit);
    set_bit(KEY_7, keyboard->keybit);
    set_bit(KEY_8, keyboard->keybit);
    set_bit(KEY_9, keyboard->keybit);
    set_bit(KEY_0, keyboard->keybit);
    set_bit(KEY_MINUS, keyboard->keybit);
    set_bit(KEY_EQUAL, keyboard->keybit);
    set_bit(KEY_BACKSPACE, keyboard->keybit);
    set_bit(KEY_TAB, keyboard->keybit);
    set_bit(KEY_Q, keyboard->keybit);
    set_bit(KEY_W, keyboard->keybit);
    set_bit(KEY_E, keyboard->keybit);
    set_bit(KEY_R, keyboard->keybit);
    set_bit(KEY_T, keyboard->keybit);
    set_bit(KEY_Y, keyboard->keybit);
    set_bit(KEY_U, keyboard->keybit);
    set_bit(KEY_I, keyboard->keybit);
    set_bit(KEY_O, keyboard->keybit);
    set_bit(KEY_P, keyboard->keybit);
    set_bit(KEY_LEFTBRACE, keyboard->keybit);
    set_bit(KEY_RIGHTBRACE, keyboard->keybit);
    set_bit(KEY_BACKSLASH, keyboard->keybit);
    set_bit(KEY_CAPSLOCK, keyboard->keybit);
    set_bit(KEY_A, keyboard->keybit);
    set_bit(KEY_S, keyboard->keybit);
    set_bit(KEY_D, keyboard->keybit);
    set_bit(KEY_F, keyboard->keybit);
    set_bit(KEY_G, keyboard->keybit);
    set_bit(KEY_H, keyboard->keybit);
    set_bit(KEY_J, keyboard->keybit);
    set_bit(KEY_K, keyboard->keybit);
    set_bit(KEY_L, keyboard->keybit);
    set_bit(KEY_SEMICOLON, keyboard->keybit);
    set_bit(KEY_APOSTROPHE, keyboard->keybit);
    set_bit(KEY_ENTER, keyboard->keybit);
    set_bit(KEY_LEFTSHIFT, keyboard->keybit);
    set_bit(KEY_Z, keyboard->keybit);
    set_bit(KEY_X, keyboard->keybit);
    set_bit(KEY_C, keyboard->keybit);
    set_bit(KEY_V, keyboard->keybit);
    set_bit(KEY_B, keyboard->keybit);
    set_bit(KEY_N, keyboard->keybit);
    set_bit(KEY_M, keyboard->keybit);
    set_bit(KEY_COMMA, keyboard->keybit);
    set_bit(KEY_DOT, keyboard->keybit);
    set_bit(KEY_SLASH, keyboard->keybit);
    set_bit(KEY_RIGHTSHIFT, keyboard->keybit);
    set_bit(KEY_LEFTCTRL, keyboard->keybit);
    set_bit(KEY_LEFTMETA, keyboard->keybit);
    set_bit(KEY_LEFTALT, keyboard->keybit);
    set_bit(KEY_SPACE, keyboard->keybit);
    set_bit(KEY_RIGHTALT, keyboard->keybit);
    set_bit(KEY_DELETE, keyboard->keybit);
    set_bit(KEY_UP, keyboard->keybit);
    set_bit(KEY_GRAVE, keyboard->keybit);
    set_bit(KEY_LEFT, keyboard->keybit);
    set_bit(KEY_DOWN, keyboard->keybit);
    set_bit(KEY_RIGHT, keyboard->keybit);
    set_bit(KEY_RIGHTCTRL, keyboard->keybit);

    result = input_register_device(keyboard);
    if (result != 0) {
        printk(KERN_ERR "%s: registration device error\n", DRIVER_NAME);
        return result;
    }

    printk(KERN_INFO "%s: module loaded\n", DRIVER_NAME);
    return 0;
}

static void __exit keyboard_tablet_exit(void) {
    flush_workqueue(workq);
    destroy_workqueue(workq); input_unregister_device(keyboard); usb_deregister(&tablet_driver);
    printk(KERN_INFO "%s: module unloaded\n", DRIVER_NAME);
}

module_init(keyboard_tablet_init);
module_exit(keyboard_tablet_exit);
