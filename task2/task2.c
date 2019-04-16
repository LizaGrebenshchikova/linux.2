#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

#define KEYBOARD_IRQ 1
#define DATA_REG 0x60
#define STATUS_MASK 0x80

int pressed;
spinlock_t lock;
struct task_struct* threadPrinterTask;

static irqreturn_t handleKeyboardEvent(int irq, void *dev_id)
{
    char scancode = inb(DATA_REG);
    if (scancode & STATUS_MASK)
    {
        spin_lock(&lock);
        ++pressed;
        spin_unlock(&lock);
    }

    return IRQ_HANDLED;
}

static int threadPrinter(void* data)
{
    int second = 0;
    while (!kthread_should_stop())
    {
        msleep_interruptible(1000);
	++second;
        if (second > 60)
        {
            spin_lock(&lock);
            printk(KERN_INFO "task2: pressed: %d\n", pressed);
            pressed = 0;
            second = 0;
            spin_unlock(&lock);
        }
    }
    return 0;
}

static int __init init(void)
{
    int res = request_irq(KEYBOARD_IRQ, handleKeyboardEvent, IRQF_SHARED, "Keyboard event handler", (void*)handleKeyboardEvent);
    printk(KERN_INFO "task2: Registered with %d number\n", res);

    spin_lock_init(&lock);
    pressed = 0;
    threadPrinterTask = kthread_run(threadPrinter, NULL, "Print info thread");
    return 0;
}

static void __exit exit(void)
{
    kthread_stop(threadPrinterTask);
    free_irq(KEYBOARD_IRQ, (void*)handleKeyboardEvent);
    printk(KERN_INFO "task2: Unregistered\n");
}

module_init(init);
module_exit(exit);
