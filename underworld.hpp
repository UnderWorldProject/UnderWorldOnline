//------------------------------------------------------------------------------------------------------
// 规则
// 1-发行卡牌：一级市场发行卡牌种类，卡牌存在品质分级
// 2-充值：通过充值获得隐石
// 3-档位设置：设置不同的充值档位（如5EOS=2000隐石），充值不符合档位的则无法兑换隐石
// 4-抽卡：消耗隐石进行抽卡，品质越高的几率越低，同时总发行量固定，抽完为止，此后只能在二级市场中获得
// 5-赠与：玩家可将卡牌直接赠予某个人
// 6-拍卖行：可以将卡牌寄售在拍卖行中买卖
//------------------------------------------------------------------------------------------------------
/**
 * Created by 黑暗之神KDS
 */
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <vector>

using namespace eosio;

#define EOS_SYMBOL S(4, EOS)
#define TOKEN_CONTRACT N(eosio.token)

// GLOBAL DATA
#define GLOBAL_TEST 0
// 请求抽卡的长度：每次有人抽卡则该变量值会+1
#define GLOBAL_REQUEST_DRAW_CARD_LENGTH 1
// 当前已处理的抽卡索引：每次处理完一次抽卡则该索引值会+1，同时可以根据此项来获知当前已发行的卡牌数
#define GLOBAL_HANDLE_DRAW_CARD_INDEX 2
// 玩家新抽到的卡的记录列表的长度
#define GLOBAL_NEW_CARD_RECORD_LENGTH 3
// 玩家新抽到的卡的记录列表当前的已处理到的值
#define GLOBAL_NEW_CARD_RECORD_INDEX 4


// 对应品质的卡牌数：A黑铁、B白银、C黄金、D紫水晶、E红宝石
#define GLOBAL_CARD_A_LENGTH 100
#define GLOBAL_CARD_B_LENGTH 101
#define GLOBAL_CARD_C_LENGTH 102
#define GLOBAL_CARD_D_LENGTH 103
#define GLOBAL_CARD_E_LENGTH 104

// 卡牌变更记录
#define CARD_CHANGE_TYPE_ADD_DRAW_CARD 1 // 来自抽卡的增加
#define CARD_CHANGE_TYPE_ADD_GIVE 2 // 来自给与的增加
#define CARD_CHANGE_TYPE_ADD_AH_OFF 3 // 来自拍卖行下架的增加
#define CARD_CHANGE_TYPE_ADD_AH_BUY 4 // 来自拍卖行购买的增加
#define CARD_CHANGE_TYPE_REMOVE_GIVE 5 // 给与别人后的减少
#define CARD_CHANGE_TYPE_REMOVE_AH_ON 6 // 寄售至拍卖行后的



// 最大可抽取的卡牌数
#define MAX_COST_DRAW_TIMES 21000000
// 每次抽5张卡的消费隐石数
#define DRAW_CARD_SCORE 150
// 每次处理抽卡请求的数目
#define ONE_TIMES_HANDLE_CARD_NUM 30
// 拍卖行手续费
#define AuctionHouseServiceChargePer 0.01

//------------------------------------------------------------------------------------------------------
// 通用方法
//------------------------------------------------------------------------------------------------------
vector<std::string> split(std::string strtem,char a);
//------------------------------------------------------------------------------------------------------
// 隐秘世界 头文件
//------------------------------------------------------------------------------------------------------
class underworld: public contract {

public:
  /**
   * 构造函数
   */
  underworld(account_name self);
  /**
   * 【管理员权限】设置档位
   * @param wei 这里其实 1wei = 0.0001EOS
   * @param amount 对应的隐石数
   */
	void modifyprice(account_name from, uint64_t wei, int64_t amount);
  /**
   * 【管理员权限】发行卡片模型
   * @param cardID 卡片ID
   * @param rarity 稀有度 0=黑铁卡 1=白银卡 2=黄金卡 3=紫水晶卡 4=红宝石卡
   */
  void issuecard(uint64_t cardID, uint64_t rarity);
  /**
   * 【管理员权限】发动
   * 结算当前的随机申请，双重随机碰撞产生最终结果
   * @param hash 传入进的随机种子
   */
  void launch(const checksum256& rarityhash,const checksum256& cardhash);
  /**
   * 【管理员权限】清理由抽卡新获得的卡牌记录，当同步完成这个增量时则清理
   * （由于EOS获取表数据是从0-limit段的数据，所以需要设计一个临时的增加量的组用于快速同步，
   *   否则每次同步需要将历史所有数据取出，数据多时将会非常堵塞）
   */
  void clearrecord(uint64_t index);
  /**
   * 充值
   */
	void transfer(account_name from, account_name to, asset quantity, std::string memo);  
  void gei(account_name account);
  /**
   * 抽取卡片（5张）
   * 传入5个随机种子
   */
  void drawcard(account_name caller,const checksum256& hash1,const checksum256& hash2,const checksum256& hash3,const checksum256& hash4,const checksum256& hash5);
  /**
   * 赠与卡牌
   * @param to 赠送的对象
   * @param cardID 卡牌ID
   * @param num 数量
   */
  void givecard(account_name caller,account_name to, uint64_t cardID, uint64_t num);
  /**
   * 拍卖行：上架
   * @param cardID 卡牌ID
   * @param num 数量
   * @param unitWei 单价 这里其实 1wei = 0.0001EOS
   */
  void auctionon(account_name caller,uint64_t cardID, uint64_t num, uint64_t unitWei);
  /**
   * 拍卖行：上架指定给某个人
   * @param cardID 卡牌ID
   * @param num 数量
   * @param unitWei 单价 这里其实 1wei = 0.0001EOS
   * @param onlyToPlayer 指定的人
   */
  void auctiononto(account_name caller,uint64_t cardID, uint64_t num, uint64_t unitWei, account_name onlyToPlayer);
  /**
   * 拍卖行：下架
   * @param cardID 卡牌ID
   * @param num 数量
   */
  void auctionoff(account_name caller,uint64_t auctionItemID);

  void tempinit();
private:
        /**
         * 设置全局数据
         * @param name 全局数据名称
         * @param value 值
         */
        void setGlobalData(uint64_t name,uint64_t value,bool isPlus);
        /**
         * 读取全局数据
         * @param name 全局数据名称
         * @return 返回数据
         */
        uint64_t getGlobalData(uint64_t name);
        /**
         * 交易实现方法
         */
        void _transfer(account_name account, asset eos);
        /**
         * 合并种子
         */
        uint64_t merge_seed(const checksum256& s1, const checksum256& s2);
        /**
         * 获取对应品质的一张随机的卡牌ID
         */
        uint64_t getrandcardid(const checksum256& rarityhash,const checksum256& cardhash, const checksum256& playerseed);
        /**
         * 给指定用户增加卡牌的实现方法
         */
        void _addCard(account_name account,uint64_t cardID,uint64_t num,uint64_t type);
        /**
         * 给指定用户减少卡牌的实现方法，卡牌不够会抛出错误
         */
        void _removeCard(account_name account,uint64_t cardID,uint64_t num,uint64_t type);
        /**
        * 拍卖行：购买物品
        * @param auctionItemID 拍卖行物品ID
        */
        void _auctionbuy(account_name caller,uint64_t auctionItemID,asset eos);
        void log(uint64_t cardidx,uint64_t n);
        //------------------------------------------------------------------------------------------------------
        // 数据 
        //------------------------------------------------------------------------------------------------------
        // 全局数据
        // @abi table globaldata i64
        struct globaldata{
          uint64_t id;
          uint64_t number;
          // std::string str;
          uint64_t primary_key() const { return id; }
          EOSLIB_SERIALIZE(globaldata, (id)(number))
        };
        typedef eosio::multi_index<N(globaldata), globaldata> globaldata_index;
        globaldata_index globaldatas;

        // 玩家资产
        // @abi table playerasset i64
        struct playerasset {
         uint128_t mykey;
         account_name owner;
         uint64_t cardID;
         uint64_t num;
         uint64_t primary_key() const { return mykey; }
         EOSLIB_SERIALIZE(playerasset, (mykey)(owner)(cardID)(num))
        };
        typedef eosio::multi_index<N(playerasset), playerasset> playerasset_index;
        playerasset_index playerassets;
        // 玩家列表
        // @abi table player i64
        struct player {
         account_name account;
         uint64_t gamescore;
         uint64_t primary_key() const { return account; }
         EOSLIB_SERIALIZE(player, (account)(gamescore))
        };
        typedef eosio::multi_index<N(player), player> player_index;
        player_index players;
        // @abi table price i64
        // 充值档位对应
        struct price {
	        uint64_t wei;
	        uint64_t amount;
	        uint64_t primary_key() const { return wei; }
	        EOSLIB_SERIALIZE(price, (wei)(amount))		
       };
       typedef eosio::multi_index<N(price), price> price_index;
       price_index prices;
      // @abi table cardmodel i64
      // 卡牌模型
      struct cardmodel{
        // 卡牌唯一ID
        uint64_t cardID;
        // 卡牌品质 0=黑铁卡 1=白银卡 2=黄金卡 3=紫水晶卡 4=红宝石卡
        uint64_t rarity;
        uint64_t primary_key() const { return cardID; }
        EOSLIB_SERIALIZE(cardmodel, (cardID)(rarity))
      };
      typedef eosio::multi_index<N(cardmodel), cardmodel> cardmodel_index;
      cardmodel_index cardmodels;

      // 卡牌组模板：按照级别归类
      #define DEF_CARD_MODEL(LEVEL) struct cardmodel##LEVEL{\
        uint64_t index;\
        uint64_t cardID;\
        uint64_t primary_key() const { return index; }\
        uint64_t get_secondary_1() const { return cardID; }\
        EOSLIB_SERIALIZE(cardmodel##LEVEL, (index)(cardID))\
      };\
      typedef eosio::multi_index<N(cardmodel##LEVEL), cardmodel##LEVEL> cardmodel##LEVEL##_index;\
      cardmodel##LEVEL##_index cardmodel##LEVEL##s;
      // 卡牌模型缓存：黑铁
      DEF_CARD_MODEL(a)
      // 卡牌模型缓存：白银
      DEF_CARD_MODEL(b)
      // 卡牌模型缓存：黄金
      DEF_CARD_MODEL(c)
      // 卡牌模型缓存：紫水晶
      DEF_CARD_MODEL(d)
      // 卡牌模型缓存：红宝石
      DEF_CARD_MODEL(e)
      // 抽卡随机种子申请
      // @abi table drawcardrd i64
      struct drawcardrd{
        uint64_t          id;
        account_name      owner;
        checksum256       seed1;
        checksum256       seed2;
        checksum256       seed3;
        checksum256       seed4;
        checksum256       seed5;
        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(drawcardrd, (id)(owner)(seed1)(seed2)(seed3)(seed4)(seed5))
      };
      typedef eosio::multi_index<N(drawcardrd), drawcardrd> drawcardrd_index;
      drawcardrd_index drawcardrds;
      // 拍卖行的卡
      // @abi table ahgoods i64
      struct ahgoods {
         uint64_t ahID;
         uint64_t price; // 1 = 1/10000 EOS
         uint64_t num;
         uint64_t cardID;
         account_name owner; // 售卖者
         account_name onlyTo; // 指定给某个人可买
         uint64_t primary_key() const { return ahID; }
         EOSLIB_SERIALIZE(ahgoods, (ahID)(price)(num)(cardID)(owner)(onlyTo))
      };
      typedef eosio::multi_index<N(ahgoods), ahgoods> ahgoods_index;
      ahgoods_index ahgoodses;
      //------------------------------------------------------------------------------------------------------
      // 增量的记录表，一般用于增量同步
      // 由于玩家资产数据量会比较大，这里使用增量来进行游戏同步 
      //------------------------------------------------------------------------------------------------------
      // @abi table cardrecord i64
      // 卡牌变更记录
      struct cardrecord{
        // 索引
        uint64_t index;
        // 类型 对应 CARD_CHANGE_TYPE_XXXXXX
        uint64_t type;
        // 拥有者
        account_name owner;
        // 卡牌唯一ID
        uint64_t cardID;
        // 数量
        uint64_t num;
        // 新获得的卡牌记录
        uint64_t primary_key() const { return index; }
        EOSLIB_SERIALIZE(cardrecord, (index)(type)(owner)(cardID)(num))
      };
      typedef eosio::multi_index<N(cardrecord), cardrecord> cardrecord_index;
      cardrecord_index cardrecords;


      // 临时测试的数据
      // @abi table tempdata i64
      struct tempdata{
        uint64_t cardidx;
        uint64_t n;
        uint64_t primary_key() const { return cardidx; }
        EOSLIB_SERIALIZE(tempdata, (cardidx)(n))
      };
      typedef eosio::multi_index<N(tempdata), tempdata> tempdata_index;
      tempdata_index tempdatas;

};