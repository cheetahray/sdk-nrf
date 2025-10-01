// filepath: src/main.c
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#define CONFIG_PAWR_CODED_PHY
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

// 外部函數聲明
extern int pawr_central_init(void);
extern int pawr_peripheral_init(uint8_t phy_type);

int main(void)
{
    printk("PAwR Broadcaster/Observer Simulation Starting...\n");
    
    // 根據編譯配置決定角色
#ifdef CONFIG_PAWR_CENTRAL_ROLE
    // Central 角色 (broadcaster)
    int err = pawr_central_init();
    if (err) {
        printk("Failed to initialize PAwR Central (err %d)\n", err);
        return err;
    }
    printk("Running as PAwR Central (Broadcaster)\n");
    
#else
    // Peripheral 角色 (observer)
    // 根據配置選擇 PHY 類型
#ifdef CONFIG_PAWR_CODED_PHY
    uint8_t phy_type = 0x04; // Coded PHY
#else
    uint8_t phy_type = 0x01; // 1M PHY
#endif
    
    int err = pawr_peripheral_init(phy_type);
    if (err) {
        printk("Failed to initialize PAwR Peripheral (err %d)\n", err);
        return err;
    }
    printk("Running as PAwR Peripheral (Observer, PHY=0x%02x)\n", phy_type);
#endif
    
    // 主循環
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}
