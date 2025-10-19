#include "bank_customer.h"
#include <iostream>
#include <vector>
#include <optional>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <fstream>
using namespace std;

class Bank{
private:
    string name;
    vector<BankCustomer> Accounts{};
    int customerCount;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & name & Accounts & customerCount;
    }

public:
    Bank(const string& name): name(name), customerCount(0){}
    
    const string& getName() const {return name;}
    const vector<BankCustomer>& listCustomers() const {return Accounts;}
    int getCustomerCount() const {return customerCount;}

    BankCustomer& createCustomer(int id, const string& cname, double initialBalance);

    BankCustomer* findById(int id);
    vector<BankCustomer*> findByName(const string& cname);

    bool saveBoost(const std::string& filename) const {
        std::ofstream ofs(filename);
        if (!ofs) return false;
        boost::archive::text_oarchive oa(ofs);
        oa << *this;
        return true;
    }

    bool loadBoost(const std::string& filename) {
        std::ifstream ifs(filename);
        if (!ifs) {
            Accounts.clear();
            customerCount = 0;
            return false; // file not found yet is OK
        }
        boost::archive::text_iarchive ia(ifs);
        ia >> *this;
        return true;
    }
};
