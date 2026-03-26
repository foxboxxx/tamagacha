#include "i2c-sw.h"

// release sda line
static inline void sda_release(i2c_dev_t* dev) {
    gpio_set_input(dev->sda);
    delay_us(dev->clk);
}

static inline void sda_low(i2c_dev_t* dev) {
    gpio_set_output(dev->sda);
    gpio_set_off(dev->sda);
    delay_us(dev->clk);
}

static inline void scl_release(i2c_dev_t* dev) {
    gpio_set_input(dev->scl);
    delay_us(dev->clk);
}

static inline void scl_low(i2c_dev_t* dev) {
    gpio_set_output(dev->scl);
    gpio_set_off(dev->scl);
    delay_us(dev->clk);
}

static int scl_is_high(i2c_dev_t* dev) {
    return gpio_read(dev->scl);
}

static int sda_is_high(i2c_dev_t* dev) {
    return gpio_read(dev->sda);
}

i2c_dev_t* i2c_init(uint32_t sda, uint32_t scl, i2c_clk_t clk) {
    i2c_dev_t* dev = kmalloc(sizeof(i2c_dev_t));
    dev->sda = sda;
    dev->scl = scl;
    dev->clk = clk;

    gpio_set_output(dev->sda);
    gpio_set_output(dev->scl);

    // gpio_set_on(dev->sda);
    // gpio_set_on(dev->scl);
    sda_release(dev);
    scl_release(dev);

    return dev;
}

void i2c_start(i2c_dev_t* dev) {
    sda_release(dev);
    scl_release(dev);
    delay_us(dev->clk);

    sda_low(dev);
    delay_us(dev->clk);

    scl_low(dev);
}

void i2c_stop(i2c_dev_t* dev) {
    scl_low(dev);
    delay_us(dev->clk);

    sda_low(dev);
    delay_us(dev->clk);

    scl_release(dev); 
    delay_us(dev->clk);

    sda_release(dev); 
    delay_us(dev->clk);
}

static int wait_for_scl_high(i2c_dev_t* dev) {
    int timeout = TIMEOUT;
    scl_release(dev);

    while (!scl_is_high(dev) && timeout--) {
        delay_us(dev->clk);
    }

    if (timeout <= 0) {
        trace("timout\n");
        return -1;
    }

    return 0;
}

// Bit writing sequence:
// 1. Set SDA to desired bit value (while SCL is LOW)
// 2. Release SCL (let it go HIGH)
// 3. Wait for clock stretching
// 4. Pull SCL back LOW
static int write_bit(i2c_dev_t* dev, int bit) {
    if (bit) sda_release(dev);
    else sda_low(dev);
    delay_us(dev->clk);

    if (wait_for_scl_high(dev) < 0) return -1;

    delay_us(dev->clk);

    scl_low(dev);
    return 0;
}

// Bit reading sequence:
// 1. Release SDA (slave may hold it low)
// 2. Release SCL (let it go HIGH)
// 3. Wait for clock stretching
// 4. Read SDA value while SCL is HIGH
// 5. Pull SCL back LOW
static int read_bit(i2c_dev_t* dev) {
    int bit;
    sda_release(dev);
    delay_us(dev->clk);
    if (wait_for_scl_high(dev) < 0) return -1;
    delay_us(dev->clk);
    bit = sda_is_high(dev);
    delay_us(dev->clk);
    scl_low(dev);
    return bit;
}

int i2c_write_byte(i2c_dev_t* dev, uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        int bit = (byte >> i) & 1;
        if (write_bit(dev, bit) < 0) return -1;
    }
    int ack = read_bit(dev);
    if (ack == 0)  return 0;  // ack
    else return 1;  // nack
}

uint8_t i2c_read_byte(i2c_dev_t* dev, int send_ack) {
    uint8_t byte = 0;
    for (int i = 7; i >= 0; i--) {
        int bit = read_bit(dev);
        if (bit < 0) return -1;
        byte |= (bit << i);
    }    
    if (send_ack) write_bit(dev, 0);  // pull sda low for ack
    else  write_bit(dev, 1);  // release sda for nack
    return byte;
}

int i2c_write_n_bytes(i2c_dev_t* dev, uint8_t addr, uint8_t* data, int length) {
    i2c_start(dev);
    uint8_t addr_byte = (addr << 1) | 0; 
    if (i2c_write_byte(dev, addr_byte) != 0) {
        i2c_stop(dev);
        return -1;
    }
    for (int i = 0; i < length; i++) {
        if (i2c_write_byte(dev, data[i]) != 0) {
            i2c_stop(dev);
            return -1;
        }
    }
    i2c_stop(dev);
    return 0;
}

int i2c_read_n_bytes(i2c_dev_t* dev, uint8_t addr, uint8_t* data, int length) {        
    i2c_start(dev);
    uint8_t addr_byte = (addr << 1) | 1;
    if (i2c_write_byte(dev, addr_byte) != 0) {
        i2c_stop(dev);
        return -1;
    }
    for (int i = 0; i < length; i++) {
        int send_ack = (i < length - 1);
        int val = i2c_read_byte(dev, send_ack);
        if (val < 0) {
            i2c_stop(dev);
            return -1;
        }
        data[i] = val;
    }
    i2c_stop(dev);
    return length;
}

int i2c_write_read_reg_n(i2c_dev_t* dev, uint8_t addr, uint8_t reg, uint8_t* data, int length) {
    i2c_start(dev);

    if (i2c_write_byte(dev, (addr << 1) | 0) != 0) {
        i2c_stop(dev);
        return -1;
    }

    if (i2c_write_byte(dev, reg) != 0) {
        i2c_stop(dev);
        return -1;
    }

    i2c_start(dev); 
    if (i2c_write_byte(dev, (addr << 1) | 1) != 0) {
        i2c_stop(dev);
        return -1;
    }

    for (int i = 0; i < length; i++) {
        int send_ack = (i < length - 1);
        int val = i2c_read_byte(dev, send_ack);
        if (val < 0) {
            i2c_stop(dev);
            return -1;
        }
        data[i] = (uint8_t)val;
    }
    
    i2c_stop(dev);

    return length;
}

void i2c_scan(i2c_dev_t* dev) {
    for (uint8_t addr = 0; addr < 0x7f; addr++) {
        i2c_start(dev);
        uint8_t addr_byte = (addr << 1) | 0;
        int ack = i2c_write_byte(dev, addr_byte);
        i2c_stop(dev);
        if (ack == 0) {
            printk("dev found at address %x\n", addr);
        }
    }
}