typedef enum {
    COMMAND_RESET,
    COMMAND_CONNECT,
    COMMAND_DISCONNECT,
    COMMAND_STATUS,
    COMMAND_REGISTER,
    COMMAND_UPDATE_VALUE,
    COMMAND_UPDATE_STYLE,
    COMMAND_UPDATE_GET,

    COMMAND_LAST
} command_t;

typedef enum {
    //receive
    POS_PREFIX,
    POS_PAYLOAD_SIZE,
    POS_PAYLOAD,
    POS_CHECKSUM,
    //send
    POS_ACK,

    POS_LAST
} pos_t;

typedef struct {
//prefix omitted because is fixed
    unsigned char pos;
    unsigned char payload_size;
    unsigned char payload[LOOKER_MSG_PAYLOAD_SIZE_MAX];
    unsigned char payload_pos;
    unsigned char crc;
    unsigned char complete;
} msg_t;

static msg_t msg;

size_t stat_ack_get_failures;
size_t stat_ack_send_failures;

//prototypes
static unsigned char ack_get(void);
static void ack_send(unsigned char ack);
static void msg_begin(command_t command);
static unsigned char msg_complete(void);
static unsigned char msg_get(void);
static void msg_send(void);
static looker_exit_t ack_wait(unsigned char *aAck);

static void msg_begin(command_t command)
{
    msg.payload_size = 0;
    msg.payload[msg.payload_size++] = command;
}

static unsigned char ack_get(void)
{
    uint8_t ack;
    looker_get(&ack, 1);

    if (ack == ACK_SUCCESS)
        PRINTF("<-ACK_SUCCESS\n");
    else
    {
        PRINTF("<-ACK_FAILURE\n");
        stat_ack_get_failures++;
    }
    return ack;
}

static void ack_send(unsigned char ack)
{
    unsigned char rx;

    //empty rx buffer
    while (looker_data_available())
        looker_get(&rx, 1);

    looker_send(&ack, 1);

    if (ack == ACK_SUCCESS)
        PRINTF("->ACK_SUCCESS\n");
    else
    {
        PRINTF("->ACK_FAILURE\n");
        stat_ack_send_failures++;
    }
}

static unsigned char msg_get(void)
{
    unsigned char rx;

    while (looker_data_available())
    {
        looker_get(&rx, 1);
        switch (msg.pos) {
            case POS_PREFIX:
                msg.complete = 0;
                //check prefix
                if (rx != LOOKER_MSG_PREFIX)
                {
                    msg.pos = POS_PREFIX;
                    PRINTF("Error: msg_get: prefix\n");
                    return 1;
                }
                else
                    msg.pos++; 
            break;
            case POS_PAYLOAD_SIZE:
                //check payload size
                if (rx > LOOKER_MSG_PAYLOAD_SIZE_MAX)
                {
                    msg.pos = POS_PREFIX;
                    PRINTF("Error: msg_get: payload size\n");
                    return 2;
                }
                else
                {
                    msg.payload_size = rx;
                    msg.payload_pos = 0;
                    msg.crc = crc_8(&rx, 1);
//PRINTF1("msg: crc: %u\n", msg.crc);
                    msg.pos++; 
                }
            break;

            case POS_PAYLOAD:
                msg.payload[msg.payload_pos] = rx;
//PRINTF1("msg: rx: %u\n", msg.payload[msg.payload_pos]);
                msg.crc = update_crc_8(msg.crc, rx);
//PRINTF1("msg: crc: %u\n", msg.crc);
                if (++msg.payload_pos >= msg.payload_size)
                    msg.pos++; 
            break;

            case POS_CHECKSUM:
                //check checksum
                if (msg.crc == rx)
                {
                    msg.pos = POS_PREFIX;
                    msg.complete = 1;
                }
                else
                {
                    msg.pos = POS_PREFIX;
                    PRINTF("Error: msg_get: checksum\n");
                    return 3;
                }
            break;

            default:
                msg.pos = POS_PREFIX;
                PRINTF("Error: msg_get: pos\n");
                return 4;
            break;
        }
    }
    return LOOKER_EXIT_SUCCESS;
}

static void msg_send(void)
{
    unsigned char rx, tx, crc;

    //empty rx buffer
    while (looker_data_available())
        looker_get(&rx, 1);

    //send prefix
    tx = LOOKER_MSG_PREFIX;
    looker_send(&tx, 1);
//PRINTF1("msg: tx: %u\n", tx);

    //send payload size
    looker_send(&msg.payload_size, 1);
//PRINTF1("msg: tx: %u\n", msg.payload_size);
    crc = crc_8(&msg.payload_size, 1);
//PRINTF1("msg: crc: %u\n", crc);

    //send payload
    size_t t;
    for (t=0; t<msg.payload_size; t++)
    {
        looker_send(&msg.payload[t], 1);
//PRINTF1("msg: tx: %u\n", msg.payload[t]);
        crc = update_crc_8(crc, msg.payload[t]);
//PRINTF1("msg: crc: %u\n", crc);
    }

    //send checksum
    looker_send(&crc, sizeof(crc));
//PRINTF1("msg: tx: %u\n", crc);


#ifdef DEBUG
    switch (msg.payload[0]) {
        case COMMAND_RESET:
            PRINTF("->RESET\n");
        break;

        case COMMAND_CONNECT:
            PRINTF("->CONNECT\n");
        break;

        case COMMAND_DISCONNECT:
            PRINTF("->DISCONNECT\n");
        break;

        case COMMAND_STATUS:
            PRINTF("->STATUS\n");
        break;

        case COMMAND_REGISTER:
            PRINTF("->REGISTER\n");
        break;

        case COMMAND_UPDATE_VALUE:
            PRINTF("->UPDATE_VALUE\n");
        break;

        case COMMAND_UPDATE_STYLE:
            PRINTF("->UPDATE_STYLE\n");
        break;

        case COMMAND_UPDATE_GET:
            PRINTF("->UPDATE_GET\n");
        break;

        default:
            PRINTF("->UNKNOWN\n");
        break;
    }
#endif //DEBUG
}

static unsigned char msg_complete(void)
{
    unsigned char complete = msg.complete;
    msg.complete = 0;
    return complete;
}

looker_exit_t ack_wait(unsigned char *aAck)
{
    //wait for ack
    size_t j;
    for (j=0; j<ACK_TIMEOUT; j++)
    {
        looker_delay_1ms();
        if (looker_data_available())
        {
            *aAck = ack_get();
            break;
        }
    }

    //timeout
    if (j >= ACK_TIMEOUT)
    {
        PRINTF1("timeout: %lu\n", j);
        return LOOKER_EXIT_TIMEOUT;
    }
    else
        PRINTF1("time: %lu\n", j);

    return LOOKER_EXIT_SUCCESS;
}

