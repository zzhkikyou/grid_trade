# grid_trade
 网格交易

![image](https://github.com/zzhkikyou/grid_trade/blob/main/README_IMG/%E7%BD%91%E6%A0%BC%E5%8E%9F%E7%90%86.png)

基于网格交易原则，争取在上下波动的股市中，稳定的赚钱。

# 功能点
- 根据设定参数生成特定的网格。
- 主动向腾讯提供的免费api查询股价。
- 参照网格交易算法，判断当前股价适合买入还是卖出，并通过邮件形式通知使用者。
- 等使用者真正买入或卖出后，主动在web页面（由本软件提供）确认操作，本软件将根据用户实际的操作反馈收益。

# 使用示例
### 编译
linux x64，基于c++17

```shell
cd src
make clean && make -j
```
生成可执行文件: bin/grid_trade

### 运行
```shell
cd workspace
sh start.sh
```

### 停止运行
```shell
cd workspace
sh stop.sh
```
# 详细说明
### 进程命令行参数
- start 前台启动
- startdaemon 后台启动
- stop 停止进程

### 配置文件说明
```json
{
    "StockCode": "002415", 
    "TopStock": 60.0,
    "LowStock": 15.0,
    "InitAssert": 200000.0,
    "GridWidth": 0.06,
    "WidthCopies": 300
}
```
"StockCode" 为股票代码

"TopStock" 为网格中的最高价，若运行期股价超过它，建议全量卖出，重新创建网格

"LowStock" 为网格中的最低价，若运行期股价超过它，建议全量卖出，重新创建网格

"InitAssert" 初始资金

"GridWidth" 网格宽度，0.06意味着，网格的一格代表6%的涨幅或者跌幅

"WidthCopies" 一格所代表的份数，跨一格，就需要买入或者卖出的份数

### web端
本软件提供web端，用于确认操作，因为本软件不会直接操作股票账户，本软件只根据当前股价以及网格位置来判断是否买入或者卖出，需要用户真实买入或者卖出后，将结果反馈回来，本软件才能更新当前网格位置，提供正确的行动指令

uri: 
- /deal 操作确认页面，用于确认用户的买入和卖出行为
- /manager 管理端页面，用于查看仓位、资金信息、操作策略等

### 待解决问题
- 重复通知问题
- 平仓数据细节问题