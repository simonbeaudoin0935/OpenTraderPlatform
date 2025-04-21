#include "Order.h"

AdvancedOptions::AdvancedOptions(const QString &str)
{
    if (str == "CND") {
        type = Type::CND;
    } else if (str == "AON") {
        type = Type::AON;
    } else if (str == "TRL") {
        type = Type::TRL;
    } else if (str.startsWith("SHWQTY=")) {
        type = Type::SHWQTY;
        shwqty = 0;
    } else if (str.startsWith("DSCPR=")) {
        type = Type::DSCPR;
        dscpr = 0;
    } else if (str == "NON") {
        type = Type::NON;
    } else if (str.startsWith("PEGVAL=")) {
        type = Type::PEGVAL;
        pegval = 0;
    } else if (str == "BKO") {
        type = Type::BKO;
    } else if (str == "PSO") {
        type = Type::PSO;
    }
}

Order::Order(const QJsonObject &jsonObj, bool isUpdate) :
    isUpdate(isUpdate)
{
    accountID = jsonObj["AccountID"].toString();

    if (jsonObj.contains("AdvancedOptions")) {
        //TODO
    }


}

bool Order::isValid()
{
    //TODO actually perform some checks
    return true;
}
