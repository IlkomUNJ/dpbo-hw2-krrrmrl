#include <iostream>
#include <vector>
#include "bank_customer.h"
#include "buyer.h"
#include "seller.h"
#include "bank.h"
#include "item.h"
#include <ctime>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <utility>


vector<Buyer> buyers;
vector<seller> sellers;
Bank bank("AquaBank");

enum PrimaryPrompt{LOGIN, REGISTER, EXIT, ADMIN_LOGIN};
enum RegisterPrompt{CREATE_BUYER, CREATE_SELLER, BACK};
using namespace std;

static string todayISO() {
    using namespace chrono;
    auto now = system_clock::now();
    time_t t = system_clock::to_time_t(now);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &t);
#endif
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &lt);
    return string(buf);
}

static tm parseISO(const string& s) {
    tm tm{}; 
    sscanf(s.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    tm.tm_year -= 1900;  // tm years since 1900
    tm.tm_mon  -= 1;     // tm months 0-11
    tm.tm_hour = 12;     // avoid any DST issues
    return tm;
}

static bool withinLastKDays(const string& isoDate, int k) {
    using namespace chrono;
    tm t_tx = parseISO(isoDate);
    tm t_now = parseISO(todayISO());

    auto tp_tx  = system_clock::from_time_t(mktime(&t_tx));
    auto tp_now = system_clock::from_time_t(mktime(&t_now));
    auto diff   = duration_cast<hours>(tp_now - tp_tx).count() / 24;
    return diff >= 0 && diff < k; 
}

struct CartEntry {
    int sellerId;
    int itemId;
    string itemName;
    int quantity;
    double unitPrice;
};

static double cartTotal(const vector<CartEntry>& cart) {
    double t = 0.0;
    for (const auto& e : cart) t += e.unitPrice * e.quantity;
    return t;
}

struct StoreTransaction {
    int buyerId;
    int sellerId;
    int itemId;
    int quantity;
    double unitPrice;
    string date;   
};

static vector<StoreTransaction> g_storeTxns;

int main() {
    //create a loop prompt 
    PrimaryPrompt prompt = LOGIN;
    RegisterPrompt regPrompt = CREATE_BUYER;
    const string ADMIN_USERNAME = "root";
    const string ADMIN_PASSWORD = "toor";
    string username, password;

    bank.loadBoost("bank.txt"); 

    while (prompt != EXIT) {
        cout << "Select an option: " << endl;
        cout << "1. Login" << endl;
        cout << "2. Register" << endl;
        cout << "3. Exit" << endl;
        cout << "4. Admin Login" << endl;
        int choice;
        cin >> choice;
        prompt = static_cast<PrimaryPrompt>(choice - 1);
        switch (prompt) {
            case LOGIN:{
                cout << "Login selected." << endl;
                cout << "\n=== Login ===\n";
                cout << "1. Buyer\n";
                cout << "2. Seller\n";
                cout << "Choose: ";
                int ltype; 
                cin >> ltype;

                if (ltype == 1) {
                    // ----- Buyer login -----
                    int id;
                    cout << "Enter Buyer ID: ";
                    cin >> id;

                    Buyer* who = nullptr;
                    for (auto& b : buyers) {
                        if (b.getId() == id) { who = &b; break; }
                    }
                    if (!who) {
                        cout << "!! Buyer not found.\n";
                        break;
                    }

                    // ----- Buyer session -----
                    int bchoice = 0;
                    vector<CartEntry> cart;
                    do {
                        cout << "\n=== Buyer Session: " << who->getName() << " (ID " << who->getId() << ") ===\n";
                        cout << "1. View Bank Balance\n";
                        cout << "2. Browse Visible Items\n";
                        cout << "3. Add Item to Cart\n";
                        cout << "4. View Cart\n";
                        cout << "5. Checkout\n";        
                        cout << "6. Logout\n"; 
                        cout << "Choose: ";
                        cin >> bchoice;

                        switch (bchoice) {
                            case 1: {
                                cout << "Balance: " << who->getAccount().getBalance() << "\n";
                                break;
                            }
                            case 2: { // Browse visible items across all sellers
                            bool any = false;
                            cout << "\nID | SellerID | Name | Qty | Price\n";
                            for (const auto& s : sellers) {
                                for (const auto& it : s.getItems()) {     // seller exposes inventory
                                    // only show items that are displayed
                                    // (make sure Item::isDisplayed() exists per 12A)
                                    if (it.isDisplayed()) {
                                        any = true;
                                        cout << it.getId() << " | "
                                                << s.getId() << " | "
                                                << it.getName() << " | "
                                                << it.getQuantity() << " | "
                                                << it.getPrice() << "\n";
                                    }
                                }
                            }
                            if (!any) cout << ">> No visible items found.\n";
                            break;
                        }
                        case 3: { // Add item to cart
                            int itemId, sellerId, qty;
                            cout << "Enter Seller ID: ";
                            cin >> sellerId;
                            cout << "Enter Item ID: ";
                            cin >> itemId;
                            cout << "Quantity: ";
                            cin >> qty;

                            // locate seller + item and basic checks
                            seller* sp = nullptr;
                            for (auto& s : sellers) if (s.getId() == sellerId) { sp = &s; break; }
                            if (!sp) { cout << ">> Seller not found.\n"; break; }

                            bool found = false;
                            for (const auto& it : sp->getItems()) {
                                if (it.getId() == itemId) {
                                    if (!it.isDisplayed()) { cout << ">> Item is not visible.\n"; break; }
                                    if (qty <= 0) { cout << ">> Quantity must be positive.\n"; break; }
                                    if (qty > it.getQuantity()) { cout << ">> Not enough stock.\n"; break; }

                                    cart.push_back(CartEntry{
                                        sp->getId(),
                                        it.getId(),
                                        it.getName(),
                                        qty,
                                        it.getPrice()
                                    });
                                    cout << ">> Added to cart: " << it.getName()
                                            << " x" << qty << " @ " << it.getPrice() << "\n";
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) { /* silent if failure already printed */ }
                            break;
                        }
                        case 4: { // View Cart
                            if (cart.empty()) {
                                cout << ">> Cart is empty.\n";
                            } else {
                                cout << "\nSellerID | ItemID | Name | Qty | UnitPrice | Subtotal\n";
                                for (const auto& e : cart) {
                                    cout << e.sellerId << " | "
                                            << e.itemId << " | "
                                            << e.itemName << " | "
                                            << e.quantity << " | "
                                            << e.unitPrice << " | "
                                            << (e.unitPrice * e.quantity) << "\n";
                                }
                                cout << "TOTAL: " << cartTotal(cart) << "\n";
                            }
                            break;
                        }
                        case 5:{ // Checkout
                            if (cart.empty()) {
                                cout << ">> Cart is empty.\n";
                                break;
                            }

                            // 1) compute total
                            double total = cartTotal(cart);

                            // 2) verify balance
                            double bal = who->getAccount().getBalance();
                            if (bal < total) {
                                cout << ">> Insufficient balance. Need " << total
                                        << ", you have " << bal << ".\n";
                                break;
                            }

                            // 3) verify stock again (in case it changed since adding to cart)
                            bool stockOK = true;
                            for (const auto& e : cart) {
                                seller* sp = nullptr;
                                for (auto& s : sellers) if (s.getId() == e.sellerId) { sp = &s; break; }
                                if (!sp) { stockOK = false; cout << ">> Seller " << e.sellerId << " not found.\n"; break; }

                                bool found = false;
                                for (auto& it : sp->itemsRef()) {
                                    if (it.getId() == e.itemId) {
                                        found = true;
                                        if (e.quantity > it.getQuantity()) {
                                            cout << ">> Not enough stock for item " << it.getName()
                                                    << " (requested " << e.quantity << ", have "
                                                    << it.getQuantity() << ").\n";
                                            stockOK = false;
                                        }
                                        break;
                                    }
                                }
                                if (!found) { stockOK = false; cout << ">> Item " << e.itemId << " not found.\n"; break; }
                                if (!stockOK) break;
                            }
                            if (!stockOK) break;

                            // 4) all good → deduct balance
                            // we’ll reuse BankCustomer::withdrawBalance (returns false if not enough)
                            if (!who->getAccount().withdrawBalance(total)) {
                                cout << ">> Unexpected: withdraw failed.\n";
                                break;
                            }

                            // 5) reduce stock and record minimal transactions
                            vector<StoreTransaction> txns;
                            for (const auto& e : cart) {
                                seller* sp = nullptr;
                                for (auto& s : sellers) if (s.getId() == e.sellerId) { sp = &s; break; }

                                for (auto& it : sp->itemsRef()) {
                                    if (it.getId() == e.itemId) {
                                        it.setQuantity(it.getQuantity() - e.quantity);
                                        // record a tiny transaction line
                                        txns.push_back(StoreTransaction{
                                            who->getId(), sp->getId(), it.getId(),
                                            e.quantity, e.unitPrice, todayISO()
                                        });
                                        g_storeTxns.push_back(txns.back());
                                        break;
                                    }
                                }
                            }

                            // 6) clear cart + print a small receipt
                            cart.clear();
                            cout << "\n=== Receipt ===\n";
                            double sum = 0.0;
                            for (const auto& t : txns) {
                                cout << "Buyer#" << t.buyerId
                                        << " bought Item#" << t.itemId
                                        << " x" << t.quantity
                                        << " @ " << t.unitPrice
                                        << " from Seller#" << t.sellerId << "\n";
                                sum += t.unitPrice * t.quantity;
                            }
                            cout << "TOTAL PAID: " << sum << "\n";
                            cout << "Remaining balance: " << who->getAccount().getBalance() << "\n";
                            break;
                        }
                        case 6:
                            cout << "Logging out...\n";
                            break;
                        default:
                            cout << "Invalid choice.\n";
                            break;
                    }
                } while (bchoice != 6);
                }
                else if (ltype == 2) {
                    // ----- Seller login -----
                    int id;
                    cout << "Enter Seller ID: ";
                    cin >> id;

                    seller* who = nullptr;
                    for (auto& s : sellers) {
                        if (s.getId() == id) { who = &s; break; }   // getId() comes from Buyer base
                    }
                    if (!who) {
                        cout << "!! Seller not found.\n";
                        break;
                    }

                    // ----- Seller session -----
                    int schoice = 0;
                    do {
                        cout << "\n=== Seller Session: " << who->getName() << " (ID " << who->getId() << ") ===\n";
                        cout << "1. View Bank Balance\n";
                        cout << "2. List Items\n";
                        cout << "3. Add Item\n";
                        cout << "4. Update Item\n";
                        cout << "5. Toggle Display (Show/Hide)\n";
                        cout << "6. Logout\n";
                        cout << "Choose: ";
                        cin >> schoice;

                        switch (schoice) {
                            case 1: {
                                cout << "Balance: " << who->getAccount().getBalance() << "\n";
                                break;
                            }
                            case 2: { // List Items
                                const auto& items = who->getItems();   // seller::getItems()
                                if (items.empty()) {
                                    cout << ">> No items in inventory.\n";
                                } else {
                                    cout << "\nID | Name | Quantity | Price\n";
                                    for (const auto& it : items) {
                                        cout << it.getId() << " | "
                                                << it.getName() << " | "
                                                << it.getQuantity() << " | "
                                                << it.getPrice() << "\n";
                                    }
                                }
                                break;
                            }
                            case 3: { // Add Item
                                int id, qty;
                                string name;
                                double price;

                                cout << "New Item ID (int): ";
                                cin >> id;
                                cout << "New Item Name: ";
                                cin.ignore();
                                getline(cin, name);
                                cout << "Quantity (int): ";
                                cin >> qty;
                                cout << "Price (double): ";
                                cin >> price;

                                who->addNewItem(id, name, qty, price); // seller::addNewItem(...)
                                cout << ">> Item added.\n";
                                break;
                            }
                            case 4: { // Update Item (name/qty/price)
                            int id, qty;
                            string name;
                            double price;

                            cout << "Item ID to update: ";
                            cin >> id;
                            cout << "New Name: ";
                            cin.ignore();
                            getline(cin, name);
                            cout << "New Quantity (int): ";
                            cin >> qty;
                            cout << "New Price (double): ";
                            cin >> price;

                            who->updateItem(id, name, qty, price);  // updates if matched id :contentReference[oaicite:2]{index=2}
                            cout << ">> Item updated (if ID existed).\n";
                            break;
                        }
                        case 5: { // Toggle Display
                            int id;
                            cout << "Item ID to toggle display: ";
                            cin >> id;

                            bool found = false;
                            for (auto &it : who->itemsRef()) {
                                if (it.getId() == id) {
                                    it.setDisplay(!it.isDisplayed());
                                    found = true;
                                    cout << ">> Display set to " << (it.isDisplayed() ? "ON" : "OFF") << ".\n";
                                    break;
                                }
                            }
                            if (!found) cout << ">> Item not found.\n";
                            break;
                        }
                        case 6:
                            cout << "Logging out...\n";
                            break;
                        default:
                            cout << "Invalid choice.\n";
                            break;
                    }
                } while (schoice != 6);
                }
                else {
                    cout << "Invalid type.\n";
                }

                break;
            }
                break;
            case REGISTER:
                regPrompt = CREATE_BUYER; // reset regPrompt to CREATE_BUYER when entering register menu
                while (regPrompt != BACK){
                    cout << "Register selected. " << endl;
                    cout << "Select an option: " << endl;
                    cout << "1. Create Buyer Account" << endl;
                    cout << "2. Create Seller Account" << endl;
                    cout << "3. Back" << endl;
                    int regChoice;
                    cin >> regChoice;
                    regPrompt = static_cast<RegisterPrompt>(regChoice - 1);
                    switch (regPrompt) {
                        case CREATE_BUYER: {
                            cout << "Create Buyer Account selected." << endl;
                            // --- Register Buyer ---
                            int buyerId;
                            string buyerName;
                            cout << "Enter Buyer ID (int): ";
                            cin >> buyerId;
                            cout << "Enter Buyer Name: ";
                            cin.ignore();
                            getline(cin, buyerName);

                            cout << "Link Bank Account:\n";
                            cout << "1. Use existing bank account\n";
                            cout << "2. Create a new bank account first\n";
                            cout << "Choose: ";
                            int bankChoice;
                            cin >> bankChoice;

                            BankCustomer* linked = nullptr;
                            if (bankChoice == 1) {
                                int bid;
                                cout << "Enter existing Bank Customer ID: ";
                                cin >> bid;
                                linked = bank.findById(bid);
                                if (!linked) {
                                    cout << "!! Bank customer not found. Registration aborted.\n";
                                    break;
                                }
                            }else if (bankChoice == 2) {
                                int bankId;
                                string bankName;
                                double initialBalance;
                                cout << "New Bank Customer ID (int): ";
                                cin >> bankId;
                                cout << "New Bank Customer Name: ";
                                cin.ignore();
                                getline(cin, bankName);
                                cout << "Initial Balance: ";
                                cin >> initialBalance;

                                BankCustomer& bc = bank.createCustomer(bankId, bankName, initialBalance);
                                linked = &bc;
                                cout << ">> Created Bank Account: ID=" << bc.getId()
                                        << " | Name=" << bc.getName()
                                        << " | Balance=" << bc.getBalance() << "\n";
                            } else {
                                cout << "Invalid choice. Registration aborted.\n";
                                break;
                            }

                            buyers.emplace_back(buyerId, buyerName, *linked);
                            cout << ">> Registered Buyer: ID=" << buyerId
                                    << " | Name=" << buyerName
                                    << " | BankID=" << linked->getId()
                                    << " | Balance=" << linked->getBalance() << "\n";
                        
                        break;}
                        case CREATE_SELLER:{
                            cout << "Create Seller Account selected." << endl;
                            // --- Register Seller ---
                            int sellerId;
                            string sellerName;
                            cout << "Enter Seller ID (int): ";
                            cin >> sellerId;
                            cout << "Enter Seller Name: ";
                            cin.ignore();
                            getline(cin, sellerName);

                            cout << "Link Bank Account:\n";
                            cout << "1. Use existing bank account\n";
                            cout << "2. Create a new bank account first\n";
                            cout << "Choose: ";
                            int bankChoice;
                            cin >> bankChoice;

                            BankCustomer* linked = nullptr;
                            if (bankChoice == 1) {
                                int bid;
                                cout << "Enter existing Bank Customer ID: ";
                                cin >> bid;
                                linked = bank.findById(bid);
                                if (!linked) {
                                    cout << "!! Bank customer not found. Registration aborted.\n";
                                    break;
                                }
                            } else if (bankChoice == 2) {
                                int bankId;
                                string bankName;
                                double initialBalance;
                                cout << "New Bank Customer ID (int): ";
                                cin >> bankId;
                                cout << "New Bank Customer Name: ";
                                cin.ignore();
                                getline(cin, bankName);
                                cout << "Initial Balance: ";
                                cin >> initialBalance;

                                BankCustomer& bc = bank.createCustomer(bankId, bankName, initialBalance);
                                linked = &bc;
                                cout << ">> Created Bank Account: ID=" << bc.getId()
                                        << " | Name=" << bc.getName()
                                        << " | Balance=" << bc.getBalance() << "\n";
                            } else {
                                cout << "Invalid choice. Registration aborted.\n";
                                break;
                            }

                            // uses the seller(int id, const string& name, BankCustomer& account) ctor you added
                            sellers.emplace_back(sellerId, sellerName, *linked);
                            cout << ">> Registered Seller: ID=" << sellerId
                                    << " | Name=" << sellerName
                                    << " | BankID=" << linked->getId()
                                    << " | Balance=" << linked->getBalance() << "\n";
                        break;}
                        case BACK:
                            cout << "Back selected." << endl;
                            break;
                        default:
                            cout << "Invalid option." << endl;
                            break;
                    }
                }
                /* if register is selected then went throuhh registration process:
                1. Create a new Buyer Account
                Must provides: Name, Home Address, Phone number, Email
                2. Option to create a Seller Account (will be linked to Buyer account)
                Must Provides 1: Home Address, Phone number, Email
                Must provides 2: Store Name, Store Address, Store Phone number, Store Email
                Must provides 3: initial deposit amount
                After finished immediately logged in as Buyer/Seller
                */
                break;
            case EXIT:
                cout << "Exiting." << endl;
                break;
            case ADMIN_LOGIN: {
                
                cout << "Username: ";
                cin >> username;
                cout << "Password: ";
                cin >> password;
                /* Prompt for username & password then check the entries with our hard coded features */
                if (username == ADMIN_USERNAME && password == ADMIN_PASSWORD) {
                    cout << "Admin login successful." << endl;
                    int adminChoice;
                    /* After login create a sub prompt that provides the following features*/

                    do{
                        cout << "\n=== Admin Menu ===\n";
                        cout << "1. Account Management\n";
                        cout << "2. System Report\n";
                        cout << "3. Logout Admin\n";
                        cout << "Choose: ";
                        cin >> adminChoice;

                        switch (adminChoice)
                        {
                        case 1:{
                            cout << "Account Management selected.\n";
                            int accmChoice;
                            do{
                                cout << "\n=== Account Management ===\n";
                                cout << "1. View All Buyers\n";
                                cout << "2. View All Sellers\n";
                                cout << "3. View All Details\n";
                                cout << "4. Seek Buyer/Seller\n";
                                cout << "5. Create New Account\n";
                                cout << "6. Remove Buyer/Seller\n";
                                cout << "7. Back\n";
                                cout << "Choose: ";
                                cin >> accmChoice;

                            switch (accmChoice)
                            {
                            case 1:
                                cout << "Viewing All Buyers....\n"; 
                                if (buyers.empty()) {
                                    cout << ">> No buyers found.\n";
                                } else {
                                    cout << "\n--- List of Buyers ---\n";
                                    for (auto &b : buyers) {
                                        cout << "ID: " << b.getId() << " | Name: " << b.getName() << endl;
                                    }
                                }
                                break;
                            case 2:
                                cout << "Viewing All Sellers....\n"; 
                                if (sellers.empty()) {
                                    cout << ">> No sellers found.\n";
                                } else {
                                    cout << "\n--- List of Sellers ---\n";
                                    for (auto &b : sellers) {
                                        cout << "ID: " << b.getId() << " | Name: " << b.getName() << endl;
                                    }
                                }
                                break;
                            case 3:
                                cout << "Viewing Details....\n";
                                cout << "\n--- Buyer Details ---\n";
                                if (buyers.empty()) {
                                    cout << ">> No buyers found.\n";
                                } else {
                                    for (auto &b : buyers) {
                                        cout << "ID: " << b.getId()
                                            << " | Name: " << b.getName()
                                            << " | Balance: " << b.getAccount().getBalance() 
                                            << endl;
                                        // kalau BankCustomer ada Address/Phone/Email tinggal tambahin
                                        // cout << " | Address: " << b.getAccount().getAddress() << ...
                                    }
                                }

                                cout << "\n--- Seller Details ---\n";
                                if (sellers.empty()) {
                                    cout << ">> No sellers found.\n";
                                } else {
                                    for (auto &s : sellers) {
                                        cout << "ID: " << s.getId()
                                            << " | Name: " << s.getName()
                                            << " | Balance: " << s.getAccount().getBalance()
                                            << endl;
                                    }
                                }
                                break;
                            case 4:{
                                cout << "Seek Buyer/Seller\n";
                                int searchOption;
                                cout << "\nSeek Buyer/Seller by:\n";
                                cout << "1. ID\n";
                                cout << "2. Name\n";
                                cout << "Choose: ";
                                cin >> searchOption;

                                if (searchOption == 1) {
                                    int id;
                                    cout << "Enter ID: ";
                                    cin >> id; 
                                    bool found = false;

                                    // Cari di buyers
                                    for (auto &b : buyers) {
                                        if (b.getId() == id) {
                                            cout << "Buyer Found -> ID: " << b.getId()
                                                << " | Name: " << b.getName()
                                                << " | Balance: " << b.getAccount().getBalance() << endl;
                                            found = true;
                                        }
                                    }

                                    // Cari di sellers
                                    for (auto &s : sellers) {
                                        if (s.getId() == id) {
                                            cout << "Seller Found -> ID: " << s.getId()
                                                << " | Name: " << s.getName()
                                                << " | Balance: " << s.getAccount().getBalance() << endl;
                                            found = true;
                                        }
                                    }

                                    if (!found) cout << ">> No account with ID " << id << " found.\n";
                                }
                                else if (searchOption == 2) {
                                    string name;
                                    cout << "Enter Name: ";
                                    cin.ignore();
                                    getline(cin, name);
                                    bool found = false;

                                    for (auto &b : buyers) {
                                        if (b.getName() == name) {
                                            cout << "Buyer Found -> ID: " << b.getId()
                                                << " | Balance: " << b.getAccount().getBalance() << endl;
                                            found = true;
                                        }
                                    }
                                    for (auto &s : sellers) {
                                        if (s.getName() == name) {
                                            cout << "Seller Found -> ID: " << s.getId()
                                                << " | Balance: " << s.getAccount().getBalance() << endl;
                                            found = true;
                                        }
                                    }

                                    if (!found) cout << ">> No account with name " << name << " found.\n";
                                }
                                else {
                                    cout << ">> Invalid search option.\n";
                                }
                                break;
                            }
                            case 5: {
                                cout << "Create New Account\n";
                                cout << "Choose type:\n";
                                cout << "1. Buyer\n";
                                cout << "2. Seller\n";
                                cout << "3. Bank Account\n";
                                cout << "Choose: ";
                                int cnaChoice; 
                                cin >> cnaChoice;

                                if (cnaChoice == 3) {
                                    // --- Create Bank Account ---
                                    int id; 
                                    string cname; 
                                    double initial;
                                    cout << "Enter Bank Customer ID (int): ";
                                    cin >> id;
                                    cout << "Enter Bank Customer Name: ";
                                    cin.ignore();
                                    getline(cin, cname);
                                    cout << "Enter Initial Balance: ";
                                    cin >> initial;

                                    BankCustomer& bc = bank.createCustomer(id, cname, initial);
                                    cout << ">> Created Bank Account: ID=" << bc.getId() 
                                        << " | Name=" << bc.getName() 
                                        << " | Balance=" << bc.getBalance() << "\n";
                                } else if (cnaChoice == 1) {
                                    int buyerId;
                                    string buyerName;

                                    cout << "Enter Buyer ID (int): ";
                                    cin >> buyerId;
                                    cout << "Enter Buyer Name: ";
                                    cin.ignore();
                                    getline(cin, buyerName);

                                    cout << "Link Bank Account:\n";
                                    cout << "1. Use existing bank account\n";
                                    cout << "2. Create a new bank account first\n";
                                    cout << "Choose: ";
                                    int bankChoice;
                                    cin >> bankChoice;

                                    BankCustomer* linked = nullptr;

                                    if (bankChoice == 1) {
                                        int bid;
                                        cout << "Enter existing Bank Customer ID: ";
                                        cin >> bid;
                                        linked = bank.findById(bid);
                                        if (!linked) {
                                            cout << "!! Bank customer not found. Aborting buyer creation.\n";
                                            break;
                                        }
                                    } else if (bankChoice == 2) {
                                        int bankId;
                                        string bankName;
                                        double initialBalance;
                                        cout << "New Bank Customer ID (int): ";
                                        cin >> bankId;
                                        cout << "New Bank Customer Name: ";
                                        cin.ignore();
                                        getline(cin, bankName);
                                        cout << "Initial Balance: ";
                                        cin >> initialBalance;

                                        BankCustomer& bc = bank.createCustomer(bankId, bankName, initialBalance);
                                        linked = &bc;
                                        cout << ">> Created Bank Account: ID=" << bc.getId()
                                                << " | Name=" << bc.getName()
                                                << " | Balance=" << bc.getBalance() << "\n";
                                    } else {
                                        cout << "Invalid choice. Aborting buyer creation.\n";
                                        break;
                                    }

                                    // At this point 'linked' must be valid
                                    if (!linked) {
                                        cout << "Unexpected error: no bank account linked.\n";
                                        break;
                                    }

                                    // Construct and store the Buyer (note: Buyer takes BankCustomer& in its ctor)
                                    buyers.emplace_back(buyerId, buyerName, *linked);

                                    cout << ">> Created Buyer: ID=" << buyerId
                                            << " | Name=" << buyerName
                                            << " | BankID=" << linked->getId()
                                            << " | Balance=" << linked->getBalance() << "\n";
                                } else if (cnaChoice == 2) {
                                    int sellerId;
                                    string sellerName;
                                    cout << "Enter Seller ID (int): ";
                                    cin >> sellerId;
                                    cout << "Enter Seller Name: ";
                                    cin.ignore();
                                    getline(cin, sellerName);

                                    cout << "Link Bank Account:\n";
                                    cout << "1. Use existing bank account\n";
                                    cout << "2. Create a new bank account first\n";
                                    cout << "Choose: ";
                                    int bankChoice;
                                    cin >> bankChoice;

                                    BankCustomer* linked = nullptr;

                                    if (bankChoice == 1) {
                                        int bid;
                                        cout << "Enter existing Bank Customer ID: ";
                                        cin >> bid;
                                        linked = bank.findById(bid);
                                        if (!linked) {
                                            cout << "!! Bank customer not found. Aborting seller creation.\n";
                                            break;
                                        }
                                    } else if (bankChoice == 2) {
                                        int bankId;
                                        string bankName;
                                        double initialBalance;
                                        cout << "New Bank Customer ID (int): ";
                                        cin >> bankId;
                                        cout << "New Bank Customer Name: ";
                                        cin.ignore();
                                        getline(cin, bankName);
                                        cout << "Initial Balance: ";
                                        cin >> initialBalance;

                                        BankCustomer& bc = bank.createCustomer(bankId, bankName, initialBalance);
                                        linked = &bc;
                                        cout << ">> Created Bank Account: ID=" << bc.getId()
                                                << " | Name=" << bc.getName()
                                                << " | Balance=" << bc.getBalance() << "\n";
                                    } else {
                                        cout << "Invalid choice. Aborting seller creation.\n";
                                        break;
                                    }

                                    if (!linked) {
                                        cout << "Unexpected error: no bank account linked.\n";
                                        break;
                                    }

                                    // Construct and store the seller
                                    sellers.emplace_back(sellerId, sellerName, *linked);

                                    cout << ">> Created Seller: ID=" << sellerId
                                            << " | Name=" << sellerName
                                            << " | BankID=" << linked->getId()
                                            << " | Balance=" << linked->getBalance() << "\n";
                                } else {
                                    cout << "Invalid type.\n";
                                }
                                break;
                            }

                            case 6:
                                cout << "Remove Buyer/Seller\n";break;
                            case 7:
                                cout << "Back to Admin Menu...\n";break;
                            default:
                                cout << "Invalid choice. Please try again.\n";
                                break;
                            }
                            } while (accmChoice!= 7);
                                /*1. Account Management
                                - View All Buyers, Sellers
                                - View All details of Buyers, Sellers
                                - Seek certain buyer of seller based on Name / account Id / address / phone number
                                - Create new buyer/seller/Bank account
                                - Remove buyer/seller based on ID (all related info will be deleted)*/
                        break;}
                        case 2:{
                            cout << "System Report selected.\n";
                            int sysrepChoice;
                            do{
                                cout << "\n=== System Report ===\n";
                                cout << "1. Total number of Buyers/Sellers\n";
                                cout << "2. Total number of Banking Accounts\n";
                                cout << "3. List all Bank Customers\n";
                                cout << "4. List recent Store Transactions\n";
                                cout << "5. List transactions in the last k days\n";
                                cout << "6. Top m most frequently transacted items\n";
                                cout << "7. Most active buyers on a date\n";
                                cout << "8. Most active sellers on a date\n";
                                cout << "9. Back\n";
                                cout << "Choose: ";
                                cin >> sysrepChoice;

                            switch (sysrepChoice)
                            {
                            case 1:
                                cout << "Total number of Buyers/Sellers\n"; 

                                break;
                            case 2:
                                cout << "Total number of Banking Accounts: \n"
                                     << bank.getCustomerCount() << endl;
                                break;
                            case 3: {
                                const auto& custs = bank.listCustomers();
                                if (custs.empty()) {
                                    cout << ">> No bank customers found.\n";
                                } else {
                                    cout << "\n--- Bank Customers ---\n";
                                    for (const auto& c : custs) {
                                        cout << "ID: " << c.getId()
                                            << " | Name: " << c.getName()
                                            << " | Balance: " << c.getBalance() << '\n';
                                    }
                                }
                                break;
                            }
                            case 4: {
                                int n;
                                cout << "How many most-recent transactions to show? ";
                                cin >> n;
                                if (n <= 0) { cout << "Invalid number.\n"; break; }

                                int total = static_cast<int>(g_storeTxns.size());
                                if (total == 0) { cout << ">> No transactions yet.\n"; break; }

                                int start = max(0, total - n);
                                cout << "\nDate | BuyerID | SellerID | ItemID | Qty | UnitPrice | Subtotal\n";
                                for (int i = start; i < total; ++i) {
                                    const auto& t = g_storeTxns[i];
                                    cout << t.date << " | "
                                        << t.buyerId << " | "
                                        << t.sellerId << " | "
                                        << t.itemId << " | "
                                        << t.quantity << " | "
                                        << t.unitPrice << " | "
                                        << (t.unitPrice * t.quantity) << "\n";
                                }
                                break;
                            }
                            case 5: {
                                int k;
                                cout << "k days = ";
                                cin >> k;
                                if (k <= 0) { cout << "Invalid k.\n"; break; }

                                bool any = false;
                                cout << "\nDate | BuyerID | SellerID | ItemID | Qty | UnitPrice | Subtotal\n";
                                for (const auto& t : g_storeTxns) {
                                    if (withinLastKDays(t.date, k)) {
                                        any = true;
                                        cout << t.date << " | "
                                                << t.buyerId  << " | "
                                                << t.sellerId << " | "
                                                << t.itemId   << " | "
                                                << t.quantity << " | "
                                                << t.unitPrice << " | "
                                                << (t.unitPrice * t.quantity) << "\n";
                                    }
                                }
                                if (!any) cout << ">> No transactions in the last " << k << " days.\n";
                                break;
                            }
                            case 6: {
                                int m;
                                cout << "m = ";
                                cin >> m;
                                if (m <= 0) { cout << "Invalid m.\n"; break; }

                                if (g_storeTxns.empty()) {
                                    cout << ">> No transactions yet.\n";
                                    break;
                                }

                                struct Key { int sellerId; int itemId; };
                                struct KeyHash {
                                    size_t operator()(const Key& k) const noexcept {
                                        return hash<long long>()((static_cast<long long>(k.sellerId) << 32) ^ k.itemId);
                                    }
                                };
                                struct KeyEq {
                                    bool operator()(const Key& a, const Key& b) const noexcept {
                                        return a.sellerId == b.sellerId && a.itemId == b.itemId;
                                    }
                                };

                                unordered_map<Key, long long, KeyHash, KeyEq> qtyByItem;
                                for (const auto& t : g_storeTxns) {
                                    qtyByItem[{t.sellerId, t.itemId}] += 1; // count orders 
                                }

                                // move to a vector to sort by quantity desc
                                struct Row { int sellerId; int itemId; long long orders; string name; double price; };
                                vector<Row> rows;
                                rows.reserve(qtyByItem.size());

                                auto findNamePrice = [&](int sellerId, int itemId) -> pair<string,double> {
                                    for (const auto& s : sellers) if (s.getId() == sellerId) {
                                        for (const auto& it : s.getItems()) if (it.getId() == itemId) {
                                            return {it.getName(), it.getPrice()};
                                        }
                                    }
                                    return {"(unknown)", 0.0}; // item may no longer exist or was renamed
                                };

                                for (const auto& kv : qtyByItem) {
                                    const Key& k = kv.first;
                                    long long orders = kv.second;
                                    auto np = findNamePrice(k.sellerId, k.itemId);
                                    rows.push_back(Row{k.sellerId, k.itemId, orders, np.first, np.second});
                                }

                                sort(rows.begin(), rows.end(),
                                        [](const Row& a, const Row& b){ return a.orders > b.orders; });

                                if (m > static_cast<int>(rows.size())) m = static_cast<int>(rows.size());
                                cout << "\nTop " << m << " items (by total quantity sold):\n";
                                cout << "Rank | SellerID | ItemID | Name | Orders | LastKnownPrice\n";
                                for (int i = 0; i < m; ++i) {
                                    const auto& r = rows[i];
                                    cout << (i+1) << " | "
                                            << r.sellerId << " | "
                                            << r.itemId   << " | "
                                            << r.name     << " | "
                                            << r.orders      << " | "
                                            << r.price    << "\n";
                                }

                                // NOTE: If you prefer “frequency” = number of orders (not quantity),
                                // replace the accumulation line above with: qtyByItem[{t.sellerId,t.itemId}] += 1;

                                break;
                            }

                            default:
                                cout << "Invalid choice. Please try again.\n";
                                break;
                            }
                            case 7: { // Most active buyers on a date
                                string date;
                                int n;
                                cout << "Date (YYYY-MM-DD or 'today'): ";
                                cin >> date;
                                if (date == "today") date = todayISO();
                                cout << "Top n = ";
                                cin >> n;
                                if (n <= 0) { cout << "Invalid n.\n"; break; }

                                // Count transactions per buyer for that date
                                unordered_map<int, int> countByBuyer;
                                for (const auto& t : g_storeTxns) {
                                    if (t.date == date) {
                                        countByBuyer[t.buyerId] += 1; // count orders
                                    }
                                }
                                if (countByBuyer.empty()) {
                                    cout << ">> No transactions on " << date << ".\n";
                                    break;
                                }

                                // Move to vector and sort desc
                                vector<pair<int,int>> rows;
                                rows.reserve(countByBuyer.size());
                                for (const auto& kv : countByBuyer) rows.push_back(kv);
                                sort(rows.begin(), rows.end(),
                                        [](const auto& a, const auto& b){ return a.second > b.second; });
                                if (n > (int)rows.size()) n = (int)rows.size();

                                auto buyerName = [&](int id)->string{
                                    for (const auto& b : buyers) if (b.getId() == id) return b.getName();
                                    return "(unknown)";
                                };

                                cout << "\nMost active buyers on " << date << ":\n";
                                cout << "Rank | BuyerID | Name | #Transactions\n";
                                for (int i = 0; i < n; ++i) {
                                    cout << (i+1) << " | "
                                            << rows[i].first << " | "
                                            << buyerName(rows[i].first) << " | "
                                            << rows[i].second << "\n";
                                }
                                break;
                            }

                            case 8: { // Most active sellers on a date
                                string date;
                                int n;
                                cout << "Date (YYYY-MM-DD or 'today'): ";
                                cin >> date;
                                if (date == "today") date = todayISO();
                                cout << "Top n = ";
                                cin >> n;
                                if (n <= 0) { cout << "Invalid n.\n"; break; }

                                // Count transactions per seller for that date
                                unordered_map<int, int> countBySeller;
                                for (const auto& t : g_storeTxns) {
                                    if (t.date == date) {
                                        countBySeller[t.sellerId] += 1; // count orders
                                    }
                                }
                                if (countBySeller.empty()) {
                                    cout << ">> No transactions on " << date << ".\n";
                                    break;
                                }

                                // Move to vector and sort desc
                                vector<pair<int,int>> rows;
                                rows.reserve(countBySeller.size());
                                for (const auto& kv : countBySeller) rows.push_back(kv);
                                sort(rows.begin(), rows.end(),
                                        [](const auto& a, const auto& b){ return a.second > b.second; });
                                if (n > (int)rows.size()) n = (int)rows.size();

                                auto sellerName = [&](int id)->string{
                                    for (const auto& s : sellers) if (s.getId() == id) return s.getName();
                                    return "(unknown)";
                                };

                                cout << "\nMost active sellers on " << date << ":\n";
                                cout << "Rank | SellerID | Name | #Transactions\n";
                                for (int i = 0; i < n; ++i) {
                                    cout << (i+1) << " | "
                                            << rows[i].first << " | "
                                            << sellerName(rows[i].first) << " | "
                                            << rows[i].second << "\n";
                                }
                                break;
                            }

                            } while (sysrepChoice!= 9);
                            break;}
                            /*2. System Report
                            - Total number of Buyers, Sellers
                            - Total number of Banking Accounts*/
                        case 3:
                            cout << "Logging out....\n";break;
                        default:
                            cout << "Invalid choice. Please try again.\n";
                            break;
                        }
                    } while (adminChoice!= 3);
                    
                } else {
                    cout << "Admin login failed." << endl;
                }
                break;
                }
                
                
                
            default:
                cout << "Invalid option." << endl;
                break;
        }
        cout << endl;

       bank.saveBoost("bank.txt");  
    }

    //BankCustomer customer1(1, "Alice", 1000.0);
    //Buyer buyer1(1, customer1.getName(), customer1);
    return 1;
    
}