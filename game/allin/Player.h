#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>


typedef struct queue_entry_
{
    struct queue_entry_ * pNextMsg;
    void * pObjData;
} queue_entry;

typedef struct obj_queue_
{
    int MsgCount;
    pthread_mutex_t Lock;
    queue_entry * pFirstMsg;
    queue_entry * pLastMsg;
} obj_queue;

void QueueInit(obj_queue * pQueue);

void QueueAdd(obj_queue * pQueue, void * pMsg, int size);

queue_entry * QueueGet(obj_queue * pQueue);

#ifdef DEBUG
#define TRACE(format, args...) \
    do \
    {   \
        struct timeval time;                \
        gettimeofday(&time, NULL);          \
        printf("[%6d.%0d]", (int)time.tv_sec, (int)time.tv_usec);    \
        printf("%s:%d:", __FILE__, __LINE__);\
        printf(format, ## args);\
    }while(0)

#else

void TRACE_Log(const char *file, int len, const char *fmt, ...);

#define TRACE(format, args...) \
        TRACE_Log(__FILE__, __LINE__, format, ## args);

#endif
/*****************************************************************************************
游戏流程:
1.	player向server注册自己的id和name（reg-msg）
2.	while（还有2个及以上玩家并且未超过最大局数）
    a)	发布座次信息：seat-info-msg（轮流坐庄）
    b)	强制押盲注：blind-msg
    c)	为每位牌手发两张底牌：hold-cards-msg
    d)	翻牌前喊注： inquire-msg/action-msg（多次）
    e)	发出三张公共牌：flop-msg
    f)	翻牌圈喊注：inquire-msg/action-msg（多次）
    g)	发出一张公共牌（转牌）：turn-msg
    h)	转牌圈喊注： inquire-msg/action-msg（多次）
    i)	发出一张公共牌（河牌）：river-msg
    j)	河牌圈喊注：inquire-msg/action-msg（多次）
    k)	若有两家以上未盖牌则摊牌比大小：showdown-msg
    l)	公布彩池分配结果：pot-win-msg
3.	本场比赛结束（game-over-msg）
******************************************************************************************/

/*****************************************************************************************
卡片出现的概率:
Poker Hand      牌的组合        理论出现概率        测试出现概率(3个玩家)
Royal Flush     皇家同花顺      649739:1            270:0
Straight Flush  同花顺          64973:1             270:0
Four-of-a-Kind  四条            4164:1              270:2
Full House      葫芦            693:1               270:12
Flush           同花色          508:1               270:23
------------------------------------------------------------------------------------------
Straight        顺子            254:1               270:49
Three-of-a-Kind 三条            46:1                270:48
TwoPair         二对            20:1                270:181
OnePair         一对            1.25:1              270:334
NoPair          没对子          1.002:1             270:158

超级同花顺  > 同花顺         > 四条           > 葫芦       > 同花  > 顺子     > 三条            > 两对    > 一对    > 高牌
Royal_Flush > Straight_Flush > Four_Of_A_Kind > Full_House > Flush > Straight > Three_Of_A_Kind > TwoPair > OnePair > High

策略: 加注 > 弃牌 > 跟注(过牌)

*****************************************************************************************/

/* 牌的组合类型 */
typedef enum CARD_TYPES_
{
    CARD_TYPES_None = 0,
    CARD_TYPES_High,
    CARD_TYPES_OnePair,
    CARD_TYPES_TwoPair,
    CARD_TYPES_Three_Of_A_Kind,
    CARD_TYPES_Straight,
    CARD_TYPES_Flush,
    CARD_TYPES_Full_House,
    CARD_TYPES_Four_Of_A_Kind,
    CARD_TYPES_Straight_Flush,
    CARD_TYPES_Royal_Flush,
    CARD_TYPES_Butt,
} CARD_TYPES;

/* 根据游戏流程定义的各种消息类型 */
typedef enum SER_MSG_TYPES_
{
    SER_MSG_TYPE_none = 0,
    // 服务器到player的消息
    SER_MSG_TYPE_seat_info,
    SER_MSG_TYPE_blind,
    SER_MSG_TYPE_hold_cards,
    SER_MSG_TYPE_hold_cards_inquire,
    //
    SER_MSG_TYPE_flop,
    SER_MSG_TYPE_flop_inquire,
    SER_MSG_TYPE_turn,
    SER_MSG_TYPE_turn_inquire,
    SER_MSG_TYPE_river,
    SER_MSG_TYPE_river_inquire,
    SER_MSG_TYPE_showdown,
    SER_MSG_TYPE_pot_win,
    SER_MSG_TYPE_notify,
    SER_MSG_TYPE_game_over,
    //
} SER_MSG_TYPES;

/* 卡片颜色 */
typedef enum CARD_COLOR_
{
    CARD_COLOR_Unknow,          /* 用于标识对手的未知卡片 */
    CARD_COLOR_SPADES = 1,
    CARD_COLOR_HEARTS,
    CARD_COLOR_CLUBS,
    CARD_COLOR_DIAMONDS,
    CARD_COLOR_BUTT,
} CARD_COLOR;

/* 卡片大小 */
typedef enum CARD_POINTT_
{
    CARD_POINTT_Unknow,      /* 用于标识对手的未知卡片 */
    CARD_POINTT_2 = 2,
    CARD_POINTT_3,
    CARD_POINTT_4,
    CARD_POINTT_5,
    CARD_POINTT_6,
    CARD_POINTT_7,
    CARD_POINTT_8,
    CARD_POINTT_9,
    CARD_POINTT_10,
    CARD_POINTT_J,
    CARD_POINTT_Q,
    CARD_POINTT_K,
    CARD_POINTT_A,
    CARD_POINTT_BUTT,
} CARD_POINT;

/* play card */
typedef struct CARD_
{
    CARD_POINT Point;
    CARD_COLOR Color;
} CARD;

/* 玩家的处理策略 */
typedef enum PLAYER_Action_
{
    ACTION_NONE,
    ACTION_fold,
    ACTION_check,       /* 让牌，即在前面的玩家，什么也不做，把机会给后面的玩家 */
    ACTION_call,        /* 跟进，即前面有人raise，即不re-raise，也不弃牌，则call， */
    ACTION_raise,       /* 加1倍注 */
//    ACTION_raise_5,     /* 加2-5倍注 */
//    ACTION_raise_10,    /* 加6-10倍注 */
//    ACTION_raise_20,    /* 加11-20倍注 */
//    ACTION_raise_much,  /* 加的更多 */
    ACTION_allin,
    ACTION_BUTTON,
    //
} PLAYER_Action;

typedef enum PLAYER_SEAT_TYPES_
{
    // 一局中player类型
    PLAYER_SEAT_TYPES_none,
    PLAYER_SEAT_TYPES_button,
    PLAYER_SEAT_TYPES_small_blind,
    PLAYER_SEAT_TYPES_big_blind,
} PLAYER_SEAT_TYPES;

/* 消息读取时数据结构 */
typedef struct MSG_READ_INFO_
{
    const char * pMsg;
    int MaxLen;
    int Index;
    void * pData;
} MSG_READ_INFO;

/* 玩家座位信息 */
typedef struct PLAYER_SEAT_INFO_
{
    unsigned int PlayerID;
    PLAYER_SEAT_TYPES Type;
    int Jetton;
    int Money;
} PLAYER_SEAT_INFO;

/* 每一局中的玩家座位 */
typedef struct MSG_SEAT_INFO_
{
    int PlayerNum;
    PLAYER_SEAT_INFO Players[8];
} MSG_SEAT_INFO;

/* 每一局的公开牌信息 */
typedef struct MSG_CARD_INFO_
{
    int CardNum;
    CARD Cards[5];    /* 前3张公牌，第4张转牌，第5张河牌，如果是手牌，就只有两张 */
} MSG_CARD_INFO;

typedef struct PLAYER_JETTION_INFO_
{
    unsigned int PlayerID;
    int Jetton;
} PLAYER_JETTION_INFO;

typedef struct MSG_BLIND_INFO_
{
    int BlindNum;
    PLAYER_JETTION_INFO BlindPlayers[2];    /* 只有大小盲注 */
} MSG_BLIND_INFO;

typedef struct MSG_POT_WIN_INFO_
{
    int PotWinCount;
    PLAYER_JETTION_INFO PotWin[2];  /* 存在多个人平均奖金的情况，一般两个人，多的情况不统计了，关系不大 */
} MSG_POT_WIN_INFO;

typedef struct MSG_SHOWDWON_PLAYER_CARD_
{
    int Index;          /* 选手名次 */
    unsigned int PlayerID;
    //char PlayerID[32];
    CARD HoldCards[2];
    char CardType[32];
} MSG_SHOWDWON_PLAYER_CARD;

/* 不一定每局都有show消息 */
typedef struct MSG_SHOWDWON_INFO_
{
    int CardNum;
    int PlayerNum;
    CARD PublicCards[5];                    /* 5张公牌 */
    MSG_SHOWDWON_PLAYER_CARD Players[8];    /* 选手的手牌 */
} MSG_SHOWDWON_INFO;

/* 查询玩家的inquire消息 */
typedef struct MSG_INQUIRE_PLAYER_ACTION_
{
    unsigned int PlayerID;
    int Jetton;     /* 筹码数 */
    int Money;      /* 金币数 */
    int Bet;        /* 本局下注数 */
    PLAYER_Action Action;   /* 当前动作 */
} MSG_INQUIRE_PLAYER_ACTION;

/* 询问玩家时，每次最多8个玩家，如果一次询问中，多于8个的，循环覆盖即可 */
typedef struct MSG_INQUIRE_INFO_
{
    int PlayerNum;  /* 读取inquire消息时的写入索引号 */
    int TotalPot;   /* 本局总彩池数 */
    MSG_INQUIRE_PLAYER_ACTION PlayerActions[8];
} MSG_INQUIRE_INFO;

/* 每一局信息 */
typedef struct RoundInfo_
{
    int RoundIndex;
    SER_MSG_TYPES CurrentMsgType;
    SER_MSG_TYPES RoundStatus;                /* 当前局状态 */

    int RaiseTimes;
    MSG_SEAT_INFO SeatInfo;
    MSG_CARD_INFO PublicCards;
    MSG_CARD_INFO HoldCards;
    MSG_BLIND_INFO Blind;
    MSG_SHOWDWON_INFO ShowDown;
    MSG_POT_WIN_INFO PotWin;
    //
    MSG_INQUIRE_INFO Inquires[4];   /* 手牌、公牌、转牌、河牌分别4次查询 */
    /* 不需要notify消息，在inquire中已经全部有了 */
} RoundInfo;

typedef void (* MSG_LineReader)(char Buffer[256], RoundInfo * pArg);
typedef void (* STG_Action)(RoundInfo * pArg);

/* 每个消息解析的结构 */
typedef struct MSG_NAME_TYPE_ENTRY_
{
    const char * pStartName;
    const char * pEndName;
    int NameLen;
    SER_MSG_TYPES MsgType;
    MSG_LineReader LinerReader;
    STG_Action Action;
} MSG_NAME_TYPE_ENTRY;

/* 选手模型 */
typedef struct PLAYER_
{
    unsigned int PlayerID;
    int PlayerLevel;        /* 选手个性级别, 1-9级，1激进，9保守 */

    int RoundIndex;         /* 局数 */

    /* hold时的行为 */

    /* flop时的行为 */

    /* turn时的行为 */

    /* river时的行为 */

} PLAYER;

/****************************************************************************************/

void ResponseAction(const char * pMsg, int size);

SER_MSG_TYPES Msg_GetMsgType(const char * pMsg, int MaxLen);

void Msg_Read_Ex(const char * pMsg, int MaxLen, RoundInfo * pRound);

const char * Msg_GetMsgNameByType(SER_MSG_TYPES Type);

CARD_TYPES STG_GetCardTypes(CARD *pCards, int CardNum, CARD_POINT MaxPoints[CARD_TYPES_Butt]);

const char *Msg_GetCardTypeName(CARD_TYPES Type);

const char * GetCardPointName(CARD_POINT point);

void Debug_ShowRoundInfo(RoundInfo *pRound);

const char * GetCardColorName(CARD * pCard);

void Debug_PrintChardInfo(CARD * pCard, int CardNum);

void Debug_PrintShowDown(MSG_SHOWDWON_INFO *pShowDown);

/*****************************stg 函数***********************************************************/

/* 每一局开始前，保存数据 */
void STG_SaveRoundData(RoundInfo * pRoundInfo);

void STG_SaveStudyData(void);

void STG_Init(void);

void STG_Dispose(void);

const char * STG_GetAction(RoundInfo * pRound);

const char * GetActionName(PLAYER_Action act);

/****************************************************************************************/

/*****************************player 函数***********************************************/

/****************************************************************************************/

#endif

