#ifndef BANK_CUSTOMER_H
#define BANK_CUSTOMER_H

#include <string>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>

using namespace std;

class BankCustomer {
private:
    int id;
    string name;
    double balance;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & id & name & balance;
    }

public:
    BankCustomer(int id, const string& name, double balance) : id(id), name(name), balance(balance) {
        this->id = id;
        this->name = name;
        this->balance = balance;
    }

    int getId() const;
    string getName() const;
    double getBalance() const;

    void printInfo() const;
    void setName(const string& name);
    void setBalance(double balance);
    void addBalance(double amount);
    bool withdrawBalance(double amount);
};

#endif // BANK_CUSTOMER_H