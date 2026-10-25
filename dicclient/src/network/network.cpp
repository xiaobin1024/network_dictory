#include "network.h"
#include<QDebug>
Network::Network() {

}
 bool Network::test()
{
     QString now = CoreUtils::TimeUtils::currentTimestamp();
     qDebug()<<"now="<<now;
     if(now.isEmpty()) return false;

     return true;
}
