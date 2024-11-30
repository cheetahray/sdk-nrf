enum SenseType {
    Temperature,     // 默認值 0
    Humidity,     // 默認值 1
    Noise    // 默認值 2
};
int sensormain(void);	
uint8_t* getSensorValue(enum SenseType type);
