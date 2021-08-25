#include <linux/thread_info.h> // for current
#include <linux/workqueue.h> // for work_on_cpu
#include <linux/spinlock.h>
#include <linux/sched/mm.h>
#include <linux/cpumask.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/module.h>
#include <asm/pgtable.h>
#include <linux/types.h>
#include <linux/sched.h> // not needed
#include <linux/init.h>
#include <linux/slab.h> // mem functions
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/mm.h>

#include "inc/asm/asm_functions.h"
#include "inc/vmcs_encoding.h"
#include "inc/utils/log.h"
#include "inc/arch/msr.h"
#include "inc/ioooctls.h"
#include "inc/main.h"
#include "inc/asm.h"

MODULE_LICENSE("GPL v2");

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);
static int device_mmap(struct file *, struct vm_area_struct *);

static struct file_operations fooops =
{
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.read = device_read,
	.mmap = device_mmap,
	.unlocked_ioctl = device_ioctl,
};

//
// init and cleanup
//

int init_module(void)
{
	printk(KERN_INFO "Initializing hyper-o.\n");
	int register_error = register_chrdev('O', "hyper-o", &fooops);
	if (register_error) return register_error;

	printk(KERN_INFO "vmexit_handle: %#llx %#lx\n", (__u64) vmexit_handle, __pa(vmexit_handle));
	printk(KERN_INFO "device_open: %#llx %#lx\n", (__u64) device_open, __pa(device_open));
	printk(KERN_INFO "vm_destroy: %#llx %#lx\n", (__u64) vm_destroy, __pa(vm_destroy));

	vmx_on();

	return  0;
}

void cleanup_module(void)
{
	unregister_chrdev('O', "hyper-o");
	vmx_off();
	//jprintf(DEBUG, "Goodbye world.\n");
}

static int device_open(struct inode *inode, struct file *file)
{
  //jprintf(DEBUG, "hyper-o device opened --- CREATING VM!\n");
	vm_t *vm = vm_alloc();
	if (!vm) return -ENOMEM;
	file->private_data = vm;
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	vm_t *vm = file->private_data;

	//jprintf(DEBUG, "hyper-o device closed --- destroying VM!\n");
	if (vm)
	{
		vm_destroy(vm);
		file->private_data = NULL;
	}
	return 0;
}

static ssize_t device_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	return 0;
}

vcpu_t *get_vcpu(vm_t *vm, int id)
{
	if (id < 0 || id >= MAX_VCPUS)
	{
		jprintf(ERROR, "Bad vcpu: %d\n", id);
		return NULL;
	}

	return &vm->vcpus[id];
}

static long device_ioctl_pinned(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ooo_userspace_memory_region m;
	__u64 i;

	vm_t *vm = file->private_data;
	vcpu_t *vcpu;

	if (!vm) return -ENOENT;

	switch (cmd & ~0xff)
	{
		case OOO_MAP:
			if (copy_from_user(&m, (void *)arg, sizeof(m))) return -EINVAL;
			if (!ept_map_range(vm, m.guest_phys_addr, (void *)m.userspace_addr, m.memory_size)) return -EINVAL;
			return 0;
		case OOO_UNMAP:
			if (copy_from_user(&m, (void *)arg, sizeof(m))) return -EINVAL;
			ept_unmap_range(vm, m.guest_phys_addr, m.memory_size);
			return 0;
		case OOO_PEEK:
			vcpu = get_vcpu(vm, cmd & 0xff);
	 		if (!vcpu) return -EINVAL;
			if (copy_to_user((void *)arg, &vcpu->state, sizeof(vcpu->state))) return -EINVAL;
			return 0;
		case OOO_POKE:
			vcpu = get_vcpu(vm, cmd & 0xff);
			if (!vcpu) return -EINVAL;
			if (copy_from_user(&vcpu->state, (void *)arg, sizeof(vcpu->state))) return -EINVAL;
			return 0;
		case OOO_RUN:
			vcpu = get_vcpu(vm, cmd & 0xff);
			if (!vcpu) return -EINVAL;
			if (copy_from_user(&vcpu->state, (void *)arg, sizeof(vcpu->state))) return -EINVAL;

			do
			{
				i = vcpu_run(vcpu);
			}
			while (i == OOO_VMEXIT_HANDLED);

			if (copy_to_user((void *)arg, &vcpu->state, sizeof(vcpu->state))) return -EINVAL;
			return i >= 0 ? i : 0x1000 | (-i);
		default:
                  //jprintf(DEBUG, "hyper-o received invalid ioctl\n");
			return -ENOTTY;
	}

	return -ENOTTY;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  //jprintf(DEBUG, "hyper-o received ioctl cmd=%llx arg=%#llx on CPU %d\n", cmd, arg, smp_processor_id());

	get_cpu();
	__u64 r = device_ioctl_pinned(file, cmd, arg);
	put_cpu();

	return r;

}

//static vm_fault_t ooo_vcpu_fault(struct vm_fault *vmf)
//{
//    	vm_t *vm = vmf->vma->vm_file->private_data;
//	vmf->page = virt_to_page(guest_to_host(vm, vmf->pgoff));
//	return 0;
//}

static int device_mmap(struct file *file, struct vm_area_struct *vma)
{
	vm_t *vm = vma->vm_file->private_data;
	unsigned long vmasize = vma->vm_end - vma->vm_start;
	unsigned long gpa = vma->vm_pgoff << PAGE_SHIFT;
	__u64 i;

	if (gpa > 0x200000) return -EINVAL;

	if (virt_to_phys(guest_to_host(vm, gpa)))
	{
          //jprintf(DEBUG, "Recycling gentlemap.");
		for (i = 0; i < vmasize; i += PAGE_SIZE)
		{
			__u64 hpa = virt_to_phys(guest_to_host(vm, gpa+i));
			if (!hpa) continue;
			vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
			remap_pfn_range(vma, vma->vm_start + i, hpa >> PAGE_SHIFT, PAGE_SIZE, vma->vm_page_prot);
		}

		return 0;
	}
	else
	{
		void *mapped = &vm->memory + gpa;
		__u64 offset = (__pa(vmexit_handle)&~(PAGE_SIZE-1)) - (__pa(mapped)&~(PAGE_SIZE-1));
		//jprintf(DEBUG, "mapped new gentlemap at page offset %llx.", offset);

		if (!ept_map_range(vm, gpa, mapped, vmasize)) return -EINVAL;
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)mapped) >> PAGE_SHIFT, vmasize, vma->vm_page_prot);
	}
	return 0;
}
