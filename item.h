#ifndef ITEM_H
#define ITEM_H

#include <string>

using namespace std;

class Item {
private:
    int id;
    std::string name;
    int quantity;
    double price;
    bool idDisplay;

public:
    Item(int id, const std::string& name, int quantity, double price)
        : id(id), name(name), quantity(quantity), price(price) {
            idDisplay = false; // Default value for idDisplay
        }
    bool isDisplayed() const { return idDisplay; }

    // Getters
    int getId() const { return id; }
    const std::string& getName() const { return name; }
    int getQuantity() const { return quantity; }
    double getPrice() const { return price; }
    Item *getItem() {
        return this; // Returns a pointer to the current item
    }

    // Setters
    void setId(int newId) { id = newId; }
    void setName(const std::string& newName) { name = newName; }
    void setQuantity(int newQuantity) { quantity = newQuantity; }
    void setPrice(double newPrice) { price = newPrice; }
    void setDisplay(bool display) { idDisplay = display; }

    void alterItemById(int itemId, const std::string& newName, int newQuantity, double newPrice) {
        if (id == itemId) {
            name = newName;
            quantity = newQuantity;
            price = newPrice;
        }
    }

    void updatePriceQuantity(int itemId, double newPrice, int newQuantity) {
        if (id == itemId) {
            price = newPrice;
            quantity = newQuantity;
        }
    }
};

#endif // ITEM_H