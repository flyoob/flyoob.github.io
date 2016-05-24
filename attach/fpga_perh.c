/*
* fileName: fpga_perh.c
* for gpmc fpga communication
*/
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/io.h>
#include <mach/gpio.h>
#include <linux/device.h>
#include <linux/platform_device.h>
// gpmc spec
#include <plat/gpmc.h>
// edma spec
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <mach/memory.h>
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/hardware/edma.h>

#define DRIVERNAME  "fpga"

// once_dma = 8KByte = 4*1024*16bit 
#define FPGA_FIFO_SIZE SZ_4K

/*------------------------------------------------------------------------------------------------------------------*/
// GPMC Config
#define GPMC_FIFO_SIZE SZ_16K

#define GPMC_FPGA_CS   3           // gpmc use CS 3
#define GPMC_CONFIG1_3 0x00001010  // 16bit size NOR FLASH like
#define GPMC_CONFIG2_3 0x00101080  // CSWROFFTIME=16cy   CSRDOFFTIME=16cy     CSEXTRADELAY=1
#define GPMC_CONFIG3_3 0x00000000  // ADV TIME used
#define GPMC_CONFIG4_3 0x0F031003  // WEOFFTIME=15cy     WEONTIME=3cy         OEOFFTIME=16cy       OEONTIME=3cy
#define GPMC_CONFIG5_3 0x000F1111  // RDACCESSTIME=15cy  WRCYCLETIME=1cy      RDCYCLETIME=1cy
#define GPMC_CONFIG6_3 0x0F030000  // WRACCESSTIME=15cy  WRDATAONADMUXBUS=3cy Add CYCLE2CYCLEDELAY
#define GPMC_CONFIG7_3 0x00000F42  // set up CONFIG7 and enable cs3
// chip-select mask address = MASKADDRESS = 16MB
// CS enabled
// Chip-select base address = BASEADDRESS = 0x02000000
// Access address 0x02000000-0x02FFFFFF
#define GPMC_MASKADDRESS 0x00FFFFFF  // fifo_size
#define GPMC_BASEADDRESS 0x02000000  // gpmc address
/*------------------------------------------------------------------------------------------------------------------*/
// FPGA GPIOs
#define CTRL_MODULE_BASE_ADDR    0x48140000
#define conf_gpio18              (CTRL_MODULE_BASE_ADDR + 0x0B98)
#define conf_gpio19              (CTRL_MODULE_BASE_ADDR + 0x0B9C)

#define WR_MEM_32(addr, data)    *(unsigned int*)OMAP2_L4_IO_ADDRESS(addr) = (unsigned int)(data)
#define RD_MEM_32(addr)          *(unsigned int*)OMAP2_L4_IO_ADDRESS(addr)

// delay for reset
#define _delay_ms(n)   mdelay(n) 
#define _delay_ns(n)   ndelay(n) 

// Read Point Low is Reset
#define FPGA_RRST_H gpio_set_value(18, 1);
#define FPGA_RRST_L gpio_set_value(18, 0);
/*------------------------------------------------------------------------------------------------------------------*/
// EDMA Config
#define MAX_DMA_TRANSFER_IN_BYTES  (4096*2)
#define STATIC_SHIFT               3
#define TCINTEN_SHIFT              20  
#define ITCINTEN_SHIFT             21
#define TCCHEN_SHIFT               22
#define ITCCHEN_SHIFT              23
/*------------------------------------------------------------------------------------------------------------------*/
//unsigned int fpga_buf[FPGA_FIFO_SIZE] = {0};
static unsigned long gpmc_membase = 0;
static void __iomem *fpga_membase = 0;
static int gpio[2];
dma_addr_t dmaphysdest = 0;
unsigned short *fpga_buf = NULL;
unsigned int dma_ch = 0;
static volatile int irqraised1 = 0;

static struct gpmc_timings fpga_timings = {
    /*- GPMC timing configurations -*/
    .sync_clk = 0,
    // CONFIG2 chip-select time
    .cs_on = 0,         /* Assertion time */
    .cs_rd_off = 50,    /* Read deassertion time */
    .cs_wr_off = 50,    /* Write deassertion time */
    // CONFIG3
    .adv_on = 0,
    .adv_rd_off = 0,
    .adv_wr_off = 0,
    // CONFIG4
    .we_on = 20,        /* WE assertion time */
    .we_off  = 20,      /* WE deassertion time */
    // CONFIG4
    .oe_on = 20,        /* OE assertion time */
    .oe_off = 20,       /* OE deassertion time */
    // CONFIG5
    .page_burst_access = 0,
    .access = 30,       /* Start-cycle to first data valid delay */
    .rd_cycle = 50,     /* Total read cycle time */
    .wr_cycle = 50,     /* Total write cycle time */
    // CONFIG6
    .wr_access = 0,
    .wr_data_mux_bus = 0,
};

// static         dev_t  dev;
// static struct  cdev   cdev;
// static struct  class  *gpmc_edma_class = NULL;

static void callback1(unsigned lch, u16 ch_status, void *data)
{
    switch(ch_status) {
    case DMA_COMPLETE:
        irqraised1 = 1;
        /*DMA_PRINTK ("\n From Callback 1: Channel %d status is: %u\n", lch, ch_status);*/
        break;
    case DMA_CC_ERROR:
        irqraised1 = -1;
        printk ("\nFrom Callback 1: DMA_CC_ERROR occured on Channel %d\n", lch);
        break;
    default:
        break;
    }
}

static int gpio_store(void);
static int gpio_recover(void);
static int gpio_config(void);
static int gpmc_config(void);
static int edma_config(void);

static int gpio_store(void)
{
    // store gpio pinmux
    gpio[0] = RD_MEM_32(conf_gpio18);
    gpio[1] = RD_MEM_32(conf_gpio19);
    return 0;
}

static int gpio_recover(void)
{
    // recover gpio pinmux
    WR_MEM_32(conf_gpio18, gpio[0]);
    WR_MEM_32(conf_gpio19, gpio[1]);
    gpio_free(gpio[0]);
    gpio_free(gpio[1]);
    return 0;
}

static int gpio_config(void)
{
    // config gpio direction
    WR_MEM_32(conf_gpio18, 1);           // MUXMODE=001
    gpio_request(18, "gpio18_en");       // request gpio46
    gpio_direction_output(18, 1);

    WR_MEM_32(conf_gpio19, 1);           // MUXMODE=001
    gpio_request(19, "gpio19_en");       // request gpio47
    gpio_direction_output(19, 1);

    return 0;
}

static int gpmc_config(void)
{
    // first reg gpmc_init() already called; io pinmux already configed
    // ti8168evm board_nand_init -> gpmc_nand_init
    u32 val = 0;
    int err = 0;
/*-
EXPORT_SYMBOL(gpmc_cs_write_reg);
EXPORT_SYMBOL(gpmc_cs_read_reg);
EXPORT_SYMBOL(gpmc_cs_set_timings);
-*/
    // gpmc cs disable memory
    val = gpmc_cs_read_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG7);
    val &= ~GPMC_CONFIG7_CSVALID;
    gpmc_cs_write_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG7, val);

    // disable cs3 irq
    gpmc_cs_configure(GPMC_FPGA_CS, GPMC_SET_IRQ_STATUS, 0);
    gpmc_cs_configure(GPMC_FPGA_CS, GPMC_ENABLE_IRQ, 0);

    // set config1
    gpmc_cs_write_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG1, GPMC_CONFIG1_READTYPE_ASYNC|  // set read type async
        GPMC_CONFIG1_WRITETYPE_ASYNC| // set write type async
        GPMC_CONFIG1_DEVICESIZE_16|   // set device size 16bit
        GPMC_CONFIG1_DEVICETYPE_NOR   // set device type nor
    );
    val = gpmc_cs_read_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG1);
    val &= ~GPMC_CONFIG1_MUXADDDATA;
    gpmc_cs_write_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG1, val);
 
    // set gpmc timings
    err = gpmc_cs_set_timings(GPMC_FPGA_CS, &fpga_timings);
    if(err < 0){
        printk(KERN_ERR "Unable to set gpmc timings\n");
    }

    // apply gpmc select memory
    err = gpmc_cs_request(GPMC_FPGA_CS, GPMC_FIFO_SIZE, &gpmc_membase);
    if(err < 0){
        printk(KERN_ERR "Cannot request GPMC CS\n");
        return err;
    }

 //   request_mem_region(gpmc_membase, GPMC_FIFO_SIZE, DRIVERNAME);

 //   fpga_membase = ioremap(gpmc_membase, GPMC_FIFO_SIZE);

    return err;
}

static int edma_config(void)
{
    // use AB mode, one_dma = 8KB/16bit
    static int acnt = 4096*2;
    static int bcnt = 1;
    static int ccnt = 1;

    int result = 0;
    unsigned int BRCnt = 0;
    int srcbidx = 0;
    int desbidx = 0;
    int srccidx = 0;
    int descidx = 0;
    struct edmacc_param param_set;

    printk("Initializing dma transfer...\n");

    // set dest memory
    fpga_buf  = dma_alloc_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, &dmaphysdest, 0);
    if (!fpga_buf) {
        printk ("dma_alloc_coherent failed for physdest\n");
        return -ENOMEM;
    }

    /* Set B count reload as B count. */
    BRCnt = bcnt;

    /* Setting up the SRC/DES Index */
    srcbidx = 0;
    desbidx = acnt;

    /* A Sync Transfer Mode */
    srccidx = 0;
    descidx = acnt;
 
    // gpmc channel
    result = edma_alloc_channel (52, callback1, NULL, 0);
    
    if (result < 0) {
        printk ("edma_alloc_channel failed, error:%d", result);
        return result;
    }
     
    dma_ch = result;
    edma_set_src (dma_ch, (unsigned long)(gpmc_membase), INCR, W16BIT);
    edma_set_dest (dma_ch, (unsigned long)(dmaphysdest), INCR, W16BIT);
    edma_set_src_index (dma_ch, srcbidx, srccidx);   // use fifo, set zero
    edma_set_dest_index (dma_ch, desbidx, descidx);  // A mode

    // A Sync Transfer Mode
    edma_set_transfer_params (dma_ch, acnt, bcnt, ccnt, BRCnt, ASYNC);

    /* Enable the Interrupts on Channel 1 */
    edma_read_slot (dma_ch, &param_set);
    param_set.opt |= (1 << ITCINTEN_SHIFT);
    param_set.opt |= (1 << TCINTEN_SHIFT);
    param_set.opt |= EDMA_TCC(EDMA_CHAN_SLOT(dma_ch));
    edma_write_slot (dma_ch, &param_set);

    return 0;
}

static int __init fpga_perh_init(void)
{
    unsigned int cnt;
    u32 val = 0;
    int ret = 0;
    int chk = 0;

    gpio_store();     // GPIO初始化
    gpio_config();
    gpmc_config();    // GPMC配置
    edma_config();    // EDMA配置

    for(cnt=0; cnt<7; cnt++){
        val = gpmc_cs_read_reg(GPMC_FPGA_CS, GPMC_CS_CONFIG1 + cnt*0x04);
        printk("GPMC_CS3_CONFIG_%d : [%08X]\n", cnt+1, val);
    }

    printk("Gpmc now start reading...\n");

    FPGA_RRST_L;
    _delay_ns(1);   // 1us
    FPGA_RRST_H;


    ret = edma_start(dma_ch);
    
    if (ret != 0) {
        printk ("dm8168_start_dma failed, error:%d", ret);
        return ret;
    }

    // wait for completion ISR
    while(irqraised1 == 0u){
        _delay_ms(10);
 //       break;
    }


    if (ret == 0) {
        for (cnt=0; cnt<FPGA_FIFO_SIZE; cnt++) {
//            fpga_buf[cnt] = readw(fpga_membase);
            if (fpga_buf[cnt] != cnt+1) {            // 进行数据校验
                chk = cnt+1;
                break;
            }
        }
        edma_stop(dma_ch);
        edma_free_channel(dma_ch);
    }

    if (chk == 0){
        printk ("Gpmc&edma reading sequence data check successful!\n");
    }else{
        printk ("Gpmc&edma reading data check error at: %d\n", chk);
    }

    for(cnt=0; cnt<8; cnt++){
        printk("[%04X] [%04X] [%04X] [%04X]\n", fpga_buf[cnt*4], fpga_buf[cnt*4+1], fpga_buf[cnt*4+2], fpga_buf[cnt*4+3]);
    }

//    gpmc_cs_free(GPMC_FPGA_CS);  
    return 0;
}
module_init(fpga_perh_init);

static void __exit fpga_perh_exit(void)
{
    gpio_recover();
    // free CS3
    gpmc_cs_free(GPMC_FPGA_CS); 
    dma_free_coherent (NULL, MAX_DMA_TRANSFER_IN_BYTES, fpga_buf, dmaphysdest);
    printk("fpga_perh exit!\n");
}
module_exit(fpga_perh_exit);

MODULE_LICENSE("GPL");