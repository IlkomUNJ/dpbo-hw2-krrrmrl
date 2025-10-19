#pragma once
#include "buyer.h"
#include "item.h"
#include <string>
#include <vector>

class seller : public Buyer {

private:
    // Add seller-specific private members here
    int sellerId;
    std::string sellerName;
    bool idDisplayed(int itemId) const {
        // Example implementation, can be customized
        return itemId > 0; // Assuming valid IDs are positive integers
    }

    vector<Item> items; // Assuming seller has a collection of items


public:
    // Read-only access to inventory
    const std::vector<Item>& getItems() const { return items; }
    std::vector<Item>& itemsRef() { return items; }


    seller() = default;

    seller(Buyer buyer, int sellerId, const std::string& sellerName)
        : Buyer(buyer.getId(), buyer.getName(), buyer.getAccount()), sellerId(sellerId), sellerName(sellerName) {
            Buyer::setId(buyer.getId());
        }
    
    seller(int id, const std::string& name, BankCustomer& account)
        : Buyer(id, name, account), sellerId(id), sellerName(name) {}

    virtual ~seller() = default;

    void addNewItem(int newId, const std::string& newName, int newQuantity, double newPrice) {
        Item newItem(newId, newName, newQuantity, newPrice);
        items.push_back(newItem);
    }

    void updateItem(int itemId, const std::string& newName, int newQuantity, double newPrice) {
        for (auto& item : items) {
            if (item.getId() == itemId) {
                item.alterItemById(itemId, newName, newQuantity, newPrice); // Assuming alterItemById is a method
            }
        }
    }

    void makeItemVisibleToCustomer(int itemId) {
        for (auto& item : items) {
            if (item.getId() == itemId) {
                item.setDisplay(true); // Assuming setDisplay is a method in Item class
                break;
            }
        }
    }

    // Add seller-specific members here
};