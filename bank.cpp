#include "bank.h"
using namespace std;

BankCustomer& Bank::createCustomer(int id, const string& cname, double initialBalance) {
    Accounts.emplace_back(id, cname, initialBalance);
    customerCount = static_cast<int>(Accounts.size());
    return Accounts.back();
}

BankCustomer* Bank::findById(int id){
    for (auto& c : Accounts){
        if (c.getId() == id){return &c;}
    }
    return nullptr;
}

vector<BankCustomer*> Bank::findByName(const string& cname){
    vector<BankCustomer*> out;
    for (auto& c :  Accounts) {
        if (c.getName() == cname) { out.push_back(&c); }
    }
    return out;
}