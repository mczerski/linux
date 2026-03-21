#ifndef _CXD2878_PRIV_H_
#define _CXD2878_PRIV_H_

#define AUTO         (0xFF) /* For IF_OUT_SEL and AGC_SEL, it means that the value is desided by config flags. */
								/* For RF_GAIN, it means that RF_GAIN_SEL(SubAddr:0x4E) = 1 */
#define OFFSET(ofs)  ((u8)(ofs) & 0x1F)
#define BW_6         (0x00)
#define BW_7         (0x01)
#define BW_8         (0x02)
#define BW_1_7       (0x03)

#define MAX_BIT_PRECISION  5
#define FRAC_BITMASK      0x1F
#define LOG2_10_100X            332                     /**< log2 (10) */
#define LOG2_E_100X             144                     /**< log2 (e) */

////////////////////////
 enum sony_demod_chip_id_t{
    SONY_DEMOD_CHIP_ID_UNKNOWN = 0,      /**< Unknown */
    SONY_DEMOD_CHIP_ID_CXD2856 = 0x090,  /**< CXD2856 / CXD6800(SiP) */
    SONY_DEMOD_CHIP_ID_CXD2857 = 0x091,  /**< CXD2857 */
    SONY_DEMOD_CHIP_ID_CXD2878 = 0x396,  /**< CXD2878 / CXD6801(SiP) */
    SONY_DEMOD_CHIP_ID_CXD2879 = 0x297,  /**< CXD2879 */
    SONY_DEMOD_CHIP_ID_CXD6802 = 0x197   /**< CXD6802(SiP) */
} ;
enum sony_dtv_system_t{
    SONY_DTV_SYSTEM_UNKNOWN,        /**< Unknown. */
    SONY_DTV_SYSTEM_DVBT,           /**< DVB-T */
    SONY_DTV_SYSTEM_DVBT2,          /**< DVB-T2 */
    SONY_DTV_SYSTEM_DVBC,           /**< DVB-C(J.83A) */
    SONY_DTV_SYSTEM_DVBC2,          /**< DVB-C2(J.382) */
    SONY_DTV_SYSTEM_ATSC,           /**< ATSC */
    SONY_DTV_SYSTEM_ATSC3,          /**< ATSC3.0 */
    SONY_DTV_SYSTEM_ISDBT,          /**< ISDB-T */
    SONY_DTV_SYSTEM_ISDBC,          /**< ISDB-C(J.83C) */
    SONY_DTV_SYSTEM_J83B,           /**< J.83B */
    SONY_DTV_SYSTEM_DVBS,           /**< DVB-S */
    SONY_DTV_SYSTEM_DVBS2,          /**< DVB-S2 */
    SONY_DTV_SYSTEM_ISDBS,          /**< ISDB-S */
    SONY_DTV_SYSTEM_ISDBS3,         /**< ISDB-S3 */
    SONY_DTV_SYSTEM_ANY             /**< Used for multiple system scanning / blind tuning */
} ;


 enum sony_dtv_bandwidth_t{
    SONY_DTV_BW_UNKNOWN = 0,              /**< Unknown bandwidth. */
    SONY_DTV_BW_1_7_MHZ = 1,              /**< 1.7MHz bandwidth. */
    SONY_DTV_BW_5_MHZ = 5,                /**< 5MHz bandwidth. */
    SONY_DTV_BW_6_MHZ = 6,                /**< 6MHz bandwidth. */
    SONY_DTV_BW_7_MHZ = 7,                /**< 7MHz bandwidth. */
    SONY_DTV_BW_8_MHZ = 8,                /**< 8MHz bandwidth. */

    SONY_DTV_BW_J83B_5_06_5_36_MSPS = 50, /**< For J.83B. 5.06/5.36Msps auto selection commonly used in US. */
    SONY_DTV_BW_J83B_5_60_MSPS = 51       /**< For J.83B. 5.6Msps used by SKY PerfecTV! Hikari in Japan. */
} ;

 enum sony_demod_state_t{
    SONY_DEMOD_STATE_UNKNOWN,           /**< Unknown. */
    SONY_DEMOD_STATE_SHUTDOWN,          /**< Chip is in Shutdown state. */
    SONY_DEMOD_STATE_SLEEP,             /**< Chip is in Sleep state. */
    SONY_DEMOD_STATE_ACTIVE,            /**< Chip is in Active state. */
    SONY_DEMOD_STATE_INVALID            /**< Invalid, result of an error during a state change. */
} ;
 struct sony_demod_ts_clk_configuration_t{
    u8 serialClkMode;      /**< Serial clock mode (gated or continuous) */
    u8 serialDutyMode;     /**< Serial clock duty mode (full rate or half rate) */
    u8 tsClkPeriod;        /**< TS clock period */
    u8 clkSelTSIf;         /**< TS clock frequency (low, mid or high) */
} ;
 struct sony_demod_iffreq_config_t{
    u32 configDVBT_5;              /**< DVB-T 5MHz */
    u32 configDVBT_6;              /**< DVB-T 6MHz */
    u32 configDVBT_7;              /**< DVB-T 7MHz */
    u32 configDVBT_8;              /**< DVB-T 8MHz */
    u32 configDVBT2_1_7;           /**< DVB-T2 1.7MHz */
    u32 configDVBT2_5;             /**< DVB-T2 5MHz */
    u32 configDVBT2_6;             /**< DVB-T2 6MHz */
    u32 configDVBT2_7;             /**< DVB-T2 7MHz */
    u32 configDVBT2_8;             /**< DVB-T2 8MHz */
    u32 configDVBC_6;              /**< DVB-C  6MHz */
    u32 configDVBC_7;              /**< DVB-C  7MHz */
    u32 configDVBC_8;              /**< DVB-C  8MHz */
    u32 configATSC;                /**< ATSC 1.0 */
    u32 configISDBT_6;             /**< ISDB-T 6MHz */
    u32 configISDBT_7;             /**< ISDB-T 7MHz */
    u32 configISDBT_8;             /**< ISDB-T 8MHz */
    u32 configJ83B_5_06_5_36;      /**< J.83B 5.06/5.36Msps auto selection */
    u32 configJ83B_5_60;           /**< J.83B. 5.6Msps */
} ;


#endif
