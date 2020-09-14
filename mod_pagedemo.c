#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/highmem.h>



unsigned long pagedemo_pid[2];

static struct ctl_table pagedemo_table[] = {
	{
	.procname = "pagedemo_pid",
	.maxlen = 2*sizeof(unsigned long),
	.mode = 0644,
	.data = &pagedemo_pid,
	.proc_handler = &proc_doulongvec_minmax,
	//.proc_handler = &proc_dointvec,
	},
	{}
};

static struct ctl_table_header* pagedemo_header;

static int get_pgd(struct seq_file *m, void *v){

	pgd_t *start_pgd;
        unsigned long start_phy_addr;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long page_with_flags, page_without_flags,  page_offset, phy_address;
	char *content;
	struct page *page;


	/* Try to get task structure by pid */
	struct task_struct* task = pid_task(find_vpid((pid_t)pagedemo_pid[0]), PIDTYPE_PID);
	
	/* Check is proccess exists */
	if(task == NULL){
		seq_printf(m,"Error: PID=%lu - Procces not exists\n",pagedemo_pid[0]);
		return 0;
	}

	/* Get page offset*/
	page_offset = pagedemo_pid[1] & ~PAGE_MASK;

	/* If proccess exists get pointer to PGD through the mm member */
	
	start_pgd = task->mm->pgd; /*Get PGD on kernel virtual address*/	
	start_phy_addr = __pa(start_pgd); /*Translate to phisycal address*/

	/*Get PGD entry adress of current virtual address*/
	pgd = pgd_offset(task->mm, pagedemo_pid[1]);
	if (pgd_none(*pgd)){
		seq_printf(m,"Error: VirtualAddress is not mapped in PGD\n");
                return 0;
	}

	/*Get P4D entry address of current virtual address*/
	p4d = p4d_offset(pgd,pagedemo_pid[1]);
	if (p4d_none(*p4d)){
               seq_printf(m,"Error: VirtualAddress is not mapped in P4D\n");
               return 0;

	}
	

	/*Get PUD entry address of current virtual address*/
	pud = pud_offset(p4d,pagedemo_pid[1]);
	if (pud_none(*pud)){
		seq_printf(m,"Error: VirtualAddress is not mapped in PUD\n");
                return 0;
	}


	/*Get PMD entry address of current virtual address*/
	pmd = pmd_offset(pud,pagedemo_pid[1]);
	if (pmd_none(*pmd)){
		seq_printf(m,"Error: VirtualAddress is not mapped in PMD\n");
                return 0;
	}

	/*Get PTE entry address of current virtual address*/
	pte = pte_offset_kernel(pmd,pagedemo_pid[1]);
	if (pte_none(*pte)){
		seq_printf(m,"Error: VirtualAddress is not mapped in PTE\n");
                return 0;
	}


	/*Get page struct from PTE pointer*/
	page = pte_page(*pte);
	
	/*Map page to kernel virtual adress*/
	content= (char *) kmap(page);
	
	
	/*Remove flags bits from PTE using PAGE_MASK*/
	page_with_flags = pte_val(*pte) ;
	page_without_flags = pte_val(*pte) & PAGE_MASK ;

	/*Sum page offset*/
	phy_address = page_without_flags | page_offset;

	

	seq_printf(m,"PID:%lu  VirtualAddress:%lu\n PGDStartAddress: 0x%0lx, 0x%0lx \n",pagedemo_pid[0],pagedemo_pid[1],(unsigned long)start_pgd, start_phy_addr);
	seq_printf(m,"PGD entry address: %lu\n",pgd_val(*pgd));
	seq_printf(m,"P4D entry address: %lu\n",p4d_val(*p4d));
	seq_printf(m,"PUD entry address: %lu\n",pud_val(*pud));
	seq_printf(m,"PMD entry address: %lu\n",pmd_val(*pmd));
	seq_printf(m,"PTE entry address: %lu\n",pte_val(*pte));
	
	seq_printf(m,"\n\nPAGE (with flags) entry address: %lu\n",page_with_flags);
	seq_printf(m,"PAGE (without flags) entry address: %lu\n",page_without_flags);
	seq_printf(m,"PAGE offset: %lu\n",page_offset);

	seq_printf(m,"Virtual Adress:%lu Physical Address:%lu\n",pagedemo_pid[1],phy_address);

	/*Show memory content*/	
	seq_printf(m,"Content :%s\n",content+page_offset);


	/*Unmap  memory*/
	kunmap(page);

	return 0;
}

static int pagedemo_open(struct inode *inode,struct file *filed){
	return single_open(filed,get_pgd,NULL);
}

static const struct file_operations pgdemo_fops = {
        .open           = pagedemo_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};



static int pagetable_init(void){
	printk(KERN_ALERT "Start pagetable demo , choose your PID\n");
	printk("PGDIR_SHIFT = %d\n", PGDIR_SHIFT);
	pagedemo_header=register_sysctl_table(pagedemo_table);
	if (!pagedemo_header) {
		printk(KERN_ALERT "Error: Failed to register sample_parent_table\n");
		return -EFAULT;
	}	
	proc_create("pagedemo_pid", 0600, NULL, &pgdemo_fops);

	return 0;
}


static void pagetable_exit(void){
	printk(KERN_INFO "Finish");
	unregister_sysctl_table(pagedemo_header);
	remove_proc_entry("pagedemo_pid",NULL);
}


module_init(pagetable_init);
module_exit(pagetable_exit);

MODULE_AUTHOR("William Felipe Welter");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Resolve physical memory address of a given virtual memory proccess of a given proccess and show their content");
