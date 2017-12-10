#ifndef __ATR_H
#define __ATR_H


/*****************************************************************************
* 
*****************************************************************************/
#define MAX_ATR_SIZE                33
#define UNKNOWN_PROTOCOL            20

#define SPECIFIC_MODE               1
#define NEGOTIABLE_MODE             2


#define ATR_MAX_SIZE 		33	/* Maximum size of ATR byte array */
#define ATR_MAX_HISTORICAL	15	/* Maximum number of historical bytes */
#define ATR_MAX_PROTOCOLS	7	/* Maximun number of protocols */
#define ATR_MAX_IB		4	/* Maximum number of interface bytes per protocol */

#define ATR_PROTOCOL_TYPE_T0	0	/* Protocol type T=0 */
#define ATR_PROTOCOL_TYPE_T1	1	/* Protocol type T=1 */
#define ATR_PROTOCOL_TYPE_T2	2	/* Protocol type T=2 */
#define ATR_PROTOCOL_TYPE_T3	3	/* Protocol type T=3 */
#define ATR_PROTOCOL_TYPE_T14	14	/* Protocol type T=14 */
#define ATR_PROTOCOL_TYPE_T15	15	/* Protocol type T=15 */
#define ATR_PROTOCOL_TYPE_RAW	16	/* Memory Cards Raw Protocol */


/*****************************************************************************
* 
*****************************************************************************/
typedef struct {
    unsigned char data[MAX_ATR_SIZE];    
    unsigned length;

    unsigned char TS;
    unsigned char T0;

    struct {
        unsigned char value;
        char present;
    } ib[ATR_MAX_PROTOCOLS][ATR_MAX_IB], TCK;

    int pn; /* protocol number */

    unsigned char hb[ATR_MAX_HISTORICAL];
    int hbn;
} ATR;



#endif
