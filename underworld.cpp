

#define EOSIO_ABI_PRO(TYPE, MEMBERS)                                                                                                              \
  extern "C" {                                                                                                                                    \
  void apply(uint64_t receiver, uint64_t code, uint64_t action)                                                                                   \
  {                                                                                                                                               \
    auto self = receiver;                                                                                                                         \
    if (action == N(onerror))                                                                                                                     \
    {                                                                                                                                             \
      eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account");                                        \
    }                                                                                                                                             \
    if ((code == TOKEN_CONTRACT && action == N(transfer)) || (code == self && (action != N(transfer) ))) \
    {                                                                                                                                             \
      TYPE thiscontract(self);                                                                                                                    \
      switch (action)                                                                                                                             \
      {                                                                                                                                           \
        EOSIO_API(TYPE, MEMBERS)                                                                                                                  \
      }                                                                                                                                           \
    }                                                                                                                                             \
  }                                                                                                                                               \
  }

// generate .wasm and .wast file
// EOSIO_ABI_PRO(underworld, (modifyprice)(transfer)(gei)(issuecard)(drawcard)(givecard)(auctionon)(auctiononto)(auctionoff)(buyservice)(setservice)(launch)(tempinit)(clearrecord))
EOSIO_ABI(underworld, (modifyprice)(transfer)(gei)(issuecard)(drawcard)(givecard)(auctionon)(auctiononto)(auctionoff)(launch)(tempinit)(clearrecord))

underworld::underworld(account_name self): 
contract(self),
players(_self, _self),
prices(_self, _self),
cardmodels(_self,_self),
cardmodelas(_self,_self),
cardmodelbs(_self,_self),
cardmodelcs(_self,_self),
cardmodelds(_self,_self),
cardmodeles(_self,_self),
drawcardrds(_self,_self),
playerassets(_self,_self),
tempdatas(_self,_self),
globaldatas(_self,_self),
cardrecords(_self,_self),
ahgoodses(_self,_self)
{
     
}


void underworld::modifyprice(account_name from, uint64_t wei, int64_t amount) {

   require_auth(_self);
   auto p = prices.find(wei);
   if (p == prices.end()) { // Player already exist
      p = prices.emplace(_self, [&](auto& price){
         price.wei = wei;
	     price.amount = 0;
      });    
   }
   prices.modify(p, 0, [&](auto &price) {
      price.amount = amount;
   }); 
}

void underworld::transfer(account_name from, account_name to, asset quantity, std::string memo) { 
    if (from == _self || to != _self) {
      return;
    }
    eosio_assert(quantity.is_valid(), "Invalid token transfer");
    eosio_assert(quantity.amount > 0, "Quantity must be positive");
   
    print( "Hello, ", S(4, EOS) );
    if (quantity.symbol == EOS_SYMBOL) {
       if(memo=="payment"){
           _transfer(from, quantity);
       }
       else if(memo.find("ah_buy:")!=-1){
             vector<std::string> mArr = split(memo,':');
             uint64_t auctionItemID = atoi(mArr[1].c_str());
             _auctionbuy(from,auctionItemID,quantity);
       }
     }
 }

 void underworld::_transfer(account_name account, asset eos) {
   
   require_auth(account);
   eosio_assert(eos.amount > 0, "must purchase a positive amount");
   eosio_assert(eos.symbol == EOS_SYMBOL, "only core token allowed" );    
   // 0.0001 EOS -> eos.amount =1
   auto pr = prices.find(eos.amount);
   if (pr == prices.end()) { //price not exist
	    return;
   }		
   if(pr->amount<=0){
     return;
   }
   auto p = players.find(account);
   if (p == players.end()) { 
      p = players.emplace(_self, [&](auto& player){
         player.account = account;
	     player.gamescore = 0;
      });    
   }
	
   players.modify(p, 0, [&](auto &player) {
      player.gamescore += pr->amount;
   });   
}


/**
 * 宏：将卡牌存入至某个品质分类的multi_index
 */
#define ISSUE_CARD(RARITY,CARD_LEVEL,LENGTH_NAME) if(rarity==RARITY){\
        uint64_t len = getGlobalData(LENGTH_NAME);\
        setGlobalData(LENGTH_NAME,len+1,false);\
        cardmodel##CARD_LEVEL##s.emplace(_self, [&](auto& cardmodel##CARD_LEVEL){\
            cardmodel##CARD_LEVEL.index = len;\
            cardmodel##CARD_LEVEL.cardID = cardID;\
        });\
        return;}
/**
  * 【管理员权限】发行卡片模型
  * @param cardID 卡片ID
  * @param rarity 稀有度 0=黑铁卡 1=白银卡 2=黄金卡 3=紫水晶卡 4=红宝石卡
  */
void underworld::issuecard(uint64_t cardID, uint64_t rarity){
   // 管理员权限
   require_auth(_self);
   // 已存在该卡的情况则忽略
   auto c = cardmodels.find(cardID);
   eosio_assert(c == cardmodels.end(), "card already exist");

   // 发行新卡（可查看列表）
   cardmodels.emplace(_self, [&](auto& cardmodel){
         cardmodel.cardID = cardID;
	     cardmodel.rarity = rarity;
   });
   // 存入至各自的multi_index
   ISSUE_CARD(0,a,GLOBAL_CARD_A_LENGTH)
   ISSUE_CARD(1,b,GLOBAL_CARD_B_LENGTH)
   ISSUE_CARD(2,c,GLOBAL_CARD_C_LENGTH)
   ISSUE_CARD(3,d,GLOBAL_CARD_D_LENGTH)
   ISSUE_CARD(4,e,GLOBAL_CARD_E_LENGTH)
}
/**
 * 抽取卡片（5张）
 */
void underworld::drawcard(account_name caller,const checksum256& hash1,const checksum256& hash2,const checksum256& hash3,const checksum256& hash4,const checksum256& hash5){
    require_auth(caller);
    // 当抽卡数大于发行总量时则再无法抽取，玩家只能通过拍卖行之间进行交易
    uint64_t currentCardNum = getGlobalData(GLOBAL_HANDLE_DRAW_CARD_INDEX);
    if(currentCardNum>=MAX_COST_DRAW_TIMES){
       return;
    }
    // 判断和扣除隐石
    auto p = players.find(caller);
    eosio_assert(p->gamescore >= DRAW_CARD_SCORE, "must have enouth gamescore");
    players.modify(p, 0, [&](auto &player) {
      player.gamescore -= DRAW_CARD_SCORE;
    });
    // 申请抽卡，加入至列表
    uint64_t requestID = getGlobalData(GLOBAL_REQUEST_DRAW_CARD_LENGTH);
    drawcardrds.emplace(_self, [&](auto& drawcardrd) {
      drawcardrd.id = requestID;
      drawcardrd.owner = caller;
      drawcardrd.seed1 = hash1;
      drawcardrd.seed2 = hash2;
      drawcardrd.seed3 = hash3;
      drawcardrd.seed4 = hash4;
      drawcardrd.seed5 = hash5;
    });
    // 抽卡请求+1，该值会不断增大
    setGlobalData(GLOBAL_REQUEST_DRAW_CARD_LENGTH,1,true);
}
/**
 * 赠与卡牌
 * @param to 赠送的对象
 * @param cardID 卡牌ID
 * @param num 数量
 */
void underworld::givecard(account_name caller,account_name to, uint64_t cardID, uint64_t num){
    require_auth(caller);
    // 减少我的卡牌资产，如果没有该资产则报错
    _removeCard(caller,cardID,num,CARD_CHANGE_TYPE_REMOVE_GIVE);
    // 增加到对方资产中
    _addCard(to,cardID,num,CARD_CHANGE_TYPE_ADD_GIVE);
}
/**
 * 拍卖行：上架
 * @param cardID 卡牌ID
 * @param num 数量
 * @param unitWei 单价 这里其实 1wei = 0.0001EOS
 */
void underworld::auctionon(account_name caller,uint64_t cardID, uint64_t num, uint64_t unitWei){
    // 上架，不限指定卖给谁
    auctiononto(caller,cardID,num,unitWei,0);
}
/**
 * 拍卖行：上架指定给某个人
 * @param cardID 卡牌ID
 * @param num 数量
 * @param unitWei 单价 这里其实 1wei = 0.0001EOS
 * @param onlyToPlayer 指定的人
 */
void underworld::auctiononto(account_name caller,uint64_t cardID, uint64_t num, uint64_t unitWei, account_name onlyToPlayer){
    require_auth(caller);
    // 减少我的卡牌资产，如果没有该资产则报错
    _removeCard(caller,cardID,num,CARD_CHANGE_TYPE_REMOVE_AH_ON);
    // 寄售至拍卖行
    ahgoodses.emplace(_self, [&](auto& ahgoods){
        ahgoods.ahID = ahgoodses.available_primary_key();
        ahgoods.price = unitWei;
        ahgoods.num = num;
        ahgoods.cardID = cardID;
        ahgoods.owner = caller;
        ahgoods.onlyTo = onlyToPlayer;
    });
}
/**
 * 拍卖行：下架
 * @param cardID 卡牌ID
 * @param num 数量
 */
void underworld::auctionoff(account_name caller,uint64_t auctionItemID){
    require_auth(caller);
    // 从拍卖行中减少该道具
    auto goodsItr = ahgoodses.find(auctionItemID);
    eosio_assert(goodsItr!=ahgoodses.end(), "no goods");
    eosio_assert(goodsItr->owner==caller, "have no right");
    ahgoodses.erase(goodsItr);
    // 回到宿主中
    _addCard(goodsItr->owner,goodsItr->cardID,goodsItr->num,CARD_CHANGE_TYPE_ADD_AH_OFF);
}
/**
 * 拍卖行：购买物品
 * @param auctionItemID 拍卖行物品ID
 */
void underworld::_auctionbuy(account_name caller,uint64_t auctionItemID,asset eos){
    require_auth(caller);
    auto goodsItr = ahgoodses.find(auctionItemID);
    eosio_assert(goodsItr!=ahgoodses.end(), "no goods");
    eosio_assert(goodsItr->onlyTo==0 || goodsItr->onlyTo==caller, "have no right");
    eosio_assert(eos.amount == goodsItr->price, "must purchase a positive amount");
    eosio_assert(eos.symbol == EOS_SYMBOL, "only core token allowed" );
    // 到新买家手中
    _addCard(goodsItr->owner,goodsItr->cardID,goodsItr->num,CARD_CHANGE_TYPE_ADD_AH_OFF);
    // 将钱转移到卖家手中
    uint64_t ts = (goodsItr->price * (1-AuctionHouseServiceChargePer));
    // 本地测试，暂有问题
    action(permission_level{_self, N(active)},N(eosio.token), N(transfer),
			      make_tuple(_self, caller, asset(ts, EOS_SYMBOL), std::string("Sell Card In Auction House")))
		      .send();
}
// 宏：给与一张卡牌,增加至玩家的资产中同时增量记录这张新获得的卡牌
#define GIVE_RAND_CARD(NUM) \
{\
      uint64_t cardID##NUM = getrandcardid(rarityhash,cardhash,itr->seed##NUM);\
      _addCard(p->account,cardID##NUM,1,CARD_CHANGE_TYPE_ADD_DRAW_CARD);\
}
/**
 * 【管理员权限】发动
 * 结算当前的随机申请，双重随机碰撞产生最终结果
 * @param hash 传入进的随机种子
 */
void underworld::launch(const checksum256& rarityhash,const checksum256& cardhash){
    require_auth(_self);
    uint64_t startIdx = getGlobalData(GLOBAL_HANDLE_DRAW_CARD_INDEX);
    uint64_t len = getGlobalData(GLOBAL_REQUEST_DRAW_CARD_LENGTH);
    // 获取抽卡随机请求的长度，如果当前处理的段超过一定长度则只处理ONE_TIMES_HANDLE_CARD_NUM的
    if(len-startIdx>ONE_TIMES_HANDLE_CARD_NUM){
        len = startIdx+ONE_TIMES_HANDLE_CARD_NUM;
    }
    // 没有请求的话则不处理
    uint64_t handleCardNum = len-startIdx;
    if(handleCardNum<=0)return;

    for (int i = startIdx; i < len; ++i) {
      auto itr = drawcardrds.find(i);
      auto p = players.find(itr->owner);
      if(p!=players.end()){
          setGlobalData(i+10000,1,true);
      }
      // 给与5张卡牌
      GIVE_RAND_CARD(1)
      GIVE_RAND_CARD(2)
      GIVE_RAND_CARD(3)
      GIVE_RAND_CARD(4)
      GIVE_RAND_CARD(5)
      drawcardrds.erase(itr);
    }
    setGlobalData(GLOBAL_HANDLE_DRAW_CARD_INDEX,len,false);
}
// 宏：清理记录
#define CLEAR_RECORD_LIST(RECORD_INDEX_NAME,LIST) \
    require_auth(_self);\
    uint64_t currentIndex = getGlobalData(RECORD_INDEX_NAME);\
    setGlobalData(RECORD_INDEX_NAME,index,false);\
    for(;currentIndex<index;currentIndex++){\
        auto itr = LIST.find(currentIndex);\
        LIST.erase(itr);\
    }
/**
 * 【管理员权限】清理由抽卡新获得的卡牌记录，当同步完成这个增量时则清理
 */
void underworld::clearrecord(uint64_t index){
    CLEAR_RECORD_LIST(GLOBAL_NEW_CARD_RECORD_INDEX,cardrecords);
}
/**
 * 给指定用户增加卡牌的实现方法
 */
void underworld::_addCard(account_name account,uint64_t cardID,uint64_t num,uint64_t type){
      uint128_t myAssetKey = account*1000000000+cardID;
      auto myasset = playerassets.find(myAssetKey);
      if (myasset == playerassets.end()) {
            myasset = playerassets.emplace(_self, [&](auto& playerasset){
                playerasset.mykey = myAssetKey;
                playerasset.owner = account;
                playerasset.cardID = cardID;
	            playerasset.num = 0;
            });
      }
      playerassets.modify(myasset, 0, [&](auto &playerasset) {
            playerasset.num +=num;
      });
      // 记录 add
      uint64_t currentRecordIdx = getGlobalData(GLOBAL_NEW_CARD_RECORD_LENGTH);
      setGlobalData(GLOBAL_NEW_CARD_RECORD_LENGTH,num,true);
      cardrecords.emplace(_self, [&](auto& cardrecord){
                cardrecord.index = currentRecordIdx;
                cardrecord.type = type;
                cardrecord.owner = account;
                cardrecord.cardID = cardID;
                cardrecord.num = num;
      });
}
void underworld::_removeCard(account_name account,uint64_t cardID,uint64_t num,uint64_t type){
      uint128_t myAssetKey = account*1000000000+cardID;
      auto myasset = playerassets.find(myAssetKey);
      eosio_assert(myasset!=playerassets.end() && myasset->num>=num, "must have enouth card");
      playerassets.modify(myasset, 0, [&](auto &playerasset) {
            playerasset.num -=num;
      });
      // 记录 remove
      uint64_t currentRecordIdx = getGlobalData(GLOBAL_NEW_CARD_RECORD_LENGTH);
      setGlobalData(GLOBAL_NEW_CARD_RECORD_LENGTH,num,true);
      cardrecords.emplace(_self, [&](auto& cardrecord){
                cardrecord.index = currentRecordIdx;
                cardrecord.type = type;
                cardrecord.owner = account;
                cardrecord.cardID = cardID;
                cardrecord.num = num;
      });      
}
//------------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------------
// 设置getCardID：找到对应品质的卡牌组，根据该组的长度和随机索引来获取卡牌ID
#define GETRANDCARD(LEVEL,LENGTH_NAME) uint64_t cardLen = getGlobalData(LENGTH_NAME);uint64_t cardidx = merge_seed(cardhash, playerseed) % cardLen;auto itr = cardmodel##LEVEL##s.find(cardidx);getCardID = itr->cardID;
uint64_t underworld::getrandcardid(const checksum256& rarityhash,const checksum256& cardhash, const checksum256& playerseed){
      // 确认品质用的随机数
      uint64_t raritySeed = merge_seed(rarityhash, playerseed);
      uint64_t num = raritySeed % 1000;
      uint64_t getCardID;
      //  log(9999,getCardID);
      if (num <= 650) {
          //黑铁卡
          GETRANDCARD(a,GLOBAL_CARD_A_LENGTH)
      } else if (num > 650 && num <= 900) {
          //白银卡
          GETRANDCARD(b,GLOBAL_CARD_B_LENGTH)
      } else if (num > 900 && num <= 970) {
          //黄金卡
          GETRANDCARD(c,GLOBAL_CARD_C_LENGTH)
      } else if (num > 970 && num <= 995) {
          //紫水晶卡
          GETRANDCARD(d,GLOBAL_CARD_D_LENGTH)
      } else {
          //红宝石卡
          GETRANDCARD(e,GLOBAL_CARD_E_LENGTH)
      }
      // 查询是否存在闪卡，如若存在则有一定几率成为闪卡
      auto flashcard = cardmodels.find(getCardID+100000);
      // 10%变成闪卡（ID=原卡ID+100000）即对应的闪卡 
      if(flashcard!= cardmodels.end() && (playerseed.hash[6] * rarityhash.hash[6] + cardhash.hash[6])%100<10){
          getCardID += 100000;
      }
      return getCardID;
}
//------------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------------
uint64_t underworld::merge_seed(const checksum256& s1, const checksum256& s2) {
      uint64_t hash = 0, x;
      for (int i = 0; i < 32; ++i) {
          hash ^= (s1.hash[i] ^ s2.hash[i]) << ((i & 7) << 3);
      }
      return hash;
}
//------------------------------------------------------------------------------------------------------
// private
//------------------------------------------------------------------------------------------------------
/**
 * 设置全局数据
 * @param name 全局数据名称
 * @param value 值
 */
void underworld::setGlobalData(uint64_t name,uint64_t value,bool isPlus){
        auto v = globaldatas.find(name);
        if (v == globaldatas.end()) {
            v = globaldatas.emplace(_self, [&](auto& globaldata){
                globaldata.id = name;
                globaldata.number = value;
            });
        }
        globaldatas.modify(v, 0, [&](auto &globaldata) {
             isPlus?globaldata.number+=value:globaldata.number=value;
        }); 
}
/**
 * 获取全局数据
 * @param name 全局数据名称
 */
uint64_t underworld::getGlobalData(uint64_t name){
        auto v = globaldatas.find(name);
        if (v == globaldatas.end()) {
            return 0;
        }
        return v->number;
}
// 分割字符串
vector<std::string> split(std::string strtem,char a)
    {
        vector<std::string> strvec;
 
        std::string::size_type pos1, pos2;
        pos2 = strtem.find(a);
        pos1 = 0;
        while (std::string::npos != pos2)
        {
                strvec.push_back(strtem.substr(pos1, pos2 - pos1));
                pos1 = pos2 + 1;
                pos2 = strtem.find(a, pos1);
        }
        strvec.push_back(strtem.substr(pos1));
        return strvec;
    }
//------------------------------------------------------------------------------------------------------
// temp
//------------------------------------------------------------------------------------------------------
void underworld::log(uint64_t cardidx,uint64_t n){     // clear数据
    //  while (tempdatas.begin() != tempdatas.end()) {
    //    tempdatas.erase(tempdatas.begin());
    //  }
        // 记录值
        auto p = tempdatas.find(cardidx);
        if (p == tempdatas.end()) {
            p = tempdatas.emplace(_self, [&](auto& tempdata){
                tempdata.cardidx = cardidx;
	            tempdata.n = n;
            });    
        }
        tempdatas.modify(p, 0, [&](auto &tempdata) {
             tempdata.n = n;
        }); 
}
void underworld::tempinit(){
    // cardmodela_size+=7;
    //  log(66666,cardmodela_size);
    //  cardmodela_size++;
    // log(66666,cardmodelas.find(8)->cardID);

    // 清空发行的卡牌信息
    // while (cardmodelas.begin() != cardmodelas.end()) {
    //    cardmodelas.erase(cardmodelas.begin());
    //  }
    //    while (cardmodelbs.begin() != cardmodelbs.end()) {
    //    cardmodelbs.erase(cardmodelbs.begin());
    //  }
    //    while (cardmodelcs.begin() != cardmodelcs.end()) {
    //    cardmodelcs.erase(cardmodelcs.begin());
    //  }
    //    while (cardmodelds.begin() != cardmodelds.end()) {
    //    cardmodelds.erase(cardmodelds.begin());
    //  }
    //    while (cardmodeles.begin() != cardmodeles.end()) {
    //    cardmodeles.erase(cardmodeles.begin());
    //  }
    //  while (cardmodels.begin() != cardmodels.end()) {
    //    cardmodels.erase(cardmodels.begin());
    //  }
    //  setGlobalData(GLOBAL_CARD_A_LENGTH,0,false);
    //  setGlobalData(GLOBAL_CARD_B_LENGTH,0,false);
    //  setGlobalData(GLOBAL_CARD_C_LENGTH,0,false);
    //  setGlobalData(GLOBAL_CARD_D_LENGTH,0,false);
    //  setGlobalData(GLOBAL_CARD_E_LENGTH,0,false);
         
    // 清空我的卡牌数据
    int i = 0;
    while (playerassets.begin() != playerassets.end()) {
        if(++i>100)break;
        playerassets.erase(playerassets.begin());
    }
    setGlobalData(GLOBAL_NEW_CARD_RECORD_INDEX,0,false);
    setGlobalData(GLOBAL_NEW_CARD_RECORD_LENGTH,0,false);
    i=0;
    while (cardrecords.begin() != cardrecords.end()) {
        if(++i>100)break;
        cardrecords.erase(cardrecords.begin());
    }
    // 清理我

//         int i = 0;
//    while (drawcardrds.begin() != drawcardrds.end()) {
//        if(++i>1)break;
//        drawcardrds.erase(drawcardrds.begin());
//      }

    // setGlobalData(GLOBAL_REQUEST_DRAW_CARD_LENGTH,1,false);

    //  return;

    // setGlobalData(GLOBAL_CARD_A_LENGTH,0,false);
    // while (globaldatas.begin() != globaldatas.end()) {
    //    globaldatas.erase(globaldatas.begin());
    //  }

        // auto p = globaldatas.find(1);
        // if (p == globaldatas.end()) {
        //     p = globaldatas.emplace(_self, [&](auto& globaldata){
        //         globaldata.id = globaldatas.available_primary_key();;
        //         globaldata.number = 22;
        //     });
        // }
	
        // globaldatas.modify(p, 0, [&](auto &globaldata) {
        //      globaldata.number = 66;
        //      globaldata.str = "11";
        // }); 
    
}
void underworld::gei(account_name account){
   auto p = players.find(account);
   if (p == players.end()) { 
      p = players.emplace(_self, [&](auto& player){
         player.account = account;
	     player.gamescore = 0;
      });    
   }
	
   players.modify(p, 0, [&](auto &player) {
      player.gamescore += 10000;
   });   
}
