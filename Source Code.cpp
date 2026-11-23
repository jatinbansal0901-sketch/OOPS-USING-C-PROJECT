#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <memory>

// ============================================================================
// UNIT IV (Part A): ROBUST EXCEPTION FRAMEWORK & CUSTOM I/O MANIPULATORS
// ============================================================================
namespace ReliefOps {

class ReliefException : public std::exception {
protected:
    std::string message_;
public:
    explicit ReliefException(std::string msg) : message_(std::move(msg)) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

class CargoOverweightException : public ReliefException {
public:
    CargoOverweightException(double attemptedTons, double maxCapacityTons)
        : ReliefException("CargoOverweightException: Attempted to load " +
                          std::to_string(attemptedTons) + "t exceeding capacity of " +
                          std::to_string(maxCapacityTons) + "t.") {}
};

class FuelDepletionException : public ReliefException {
public:
    FuelDepletionException(const std::string& vehicleId, double requiredKm, double rangeKm)
        : ReliefException("FuelDepletionException: Vehicle [" + vehicleId +
                          "] requires " + std::to_string(requiredKm) +
                          "km range, available range is " + std::to_string(rangeKm) + "km.") {}
};

class DamagedCorridorException : public ReliefException {
public:
    DamagedCorridorException(const std::string& sector, const std::string& reason)
        : ReliefException("DamagedCorridorException: Sector [" + sector +
                          "] impassable - " + reason) {}
};

// User-Defined Parameterless I/O Manipulators for Tabular Manifest Printing
std::ostream& manifestHeader(std::ostream& os) {
    os << "\n" << std::string(78, '=') << "\n"
       << std::left << std::setw(12) << "CARGO_ID"
       << std::setw(24) << "ITEM_NAME"
       << std::setw(14) << "CATEGORY"
       << std::right << std::setw(10) << "QTY"
       << std::setw(10) << "TONS"
       << std::setw(8)  << "PRIO" << "\n"
       << std::string(78, '-') << "\n";
    return os;
}

std::ostream& manifestDivider(std::ostream& os) {
    os << std::string(78, '-') << "\n";
    return os;
}

std::ostream& manifestFooter(std::ostream& os) {
    os << std::string(78, '=') << "\n";
    return os;
}

} // namespace ReliefOps

// ============================================================================
// UNIT I & II: INVENTORY NAMESPACE, CARGO ADT, DYNAMIC MANIFEST & DEPOT
// ============================================================================
namespace ReliefOps {
namespace Inventory {

// Forward declaration for Unit I friend class requirement
class IncidentCommander;

enum class SupplyCategory {
    RATION_KIT,
    TRAUMA_MEDICINE,
    CLEAN_WATER,
    THERMAL_BLANKET
};

inline std::string categoryToString(SupplyCategory cat) {
    switch (cat) {
        case SupplyCategory::RATION_KIT:      return "RationKit";
        case SupplyCategory::TRAUMA_MEDICINE: return "TraumaMed";
        case SupplyCategory::CLEAN_WATER:     return "WaterTank";
        case SupplyCategory::THERMAL_BLANKET: return "Blankets";
    }
    return "Unknown";
}

// Inline Utility Methods (Unit I)
inline double calculateMetricTonnage(int units, double unitWeightKg) {
    return (static_cast<double>(units) * unitWeightKg) / 1000.0;
}

inline double estimateFuelConsumption(double distanceKm, double payloadTons, double baseRateLPerKm) {
    // 4% fuel penalty per metric ton of payload
    return distanceKm * baseRateLPerKm * (1.0 + 0.04 * payloadTons);
}

// Abstract Data Type: ReliefCargo (Unit II)
class ReliefCargo {
private:
    std::string cargoId_;
    std::string itemName_;
    SupplyCategory category_;
    int quantity_;
    double unitWeightKg_;
    int triagePriority_; // 1 (Low) to 10 (Critical Life-Saving)

    friend class IncidentCommander; // Unit I: Friend access for emergency triage overrides

public:
    ReliefCargo()
        : cargoId_("NONE"), itemName_("Unassigned"), category_(SupplyCategory::RATION_KIT),
          quantity_(0), unitWeightKg_(0.0), triagePriority_(1) {}

    ReliefCargo(std::string id, std::string name, SupplyCategory cat,
                int qty, double unitKg, int priority)
        : cargoId_(std::move(id)), itemName_(std::move(name)), category_(cat),
          quantity_(qty), unitWeightKg_(unitKg), triagePriority_(priority) {}

    // Getters
    const std::string& getId() const { return cargoId_; }
    const std::string& getName() const { return itemName_; }
    SupplyCategory getCategory() const { return category_; }
    int getQuantity() const { return quantity_; }
    double getUnitWeightKg() const { return unitWeightKg_; }
    int getPriority() const { return triagePriority_; }
    double getTotalTons() const { return calculateMetricTonnage(quantity_, unitWeightKg_); }

    void adjustQuantity(int delta) { quantity_ += delta; }

    // Operator Overloading: == (Unit II: Verify cargo/manifest identity & duplicates)
    bool operator==(const ReliefCargo& other) const {
        return cargoId_ == other.cargoId_ && category_ == other.category_;
    }

    // Operator Overloading: << and >> for Standard I/O & Serialization (Unit II)
    friend std::ostream& operator<<(std::ostream& os, const ReliefCargo& c) {
        os << std::left << std::setw(12) << c.cargoId_
           << std::setw(24) << c.itemName_
           << std::setw(14) << categoryToString(c.category_)
           << std::right << std::setw(10) << c.quantity_
           << std::fixed << std::setprecision(2) << std::setw(10) << c.getTotalTons()
           << std::setw(8) << c.triagePriority_;
        return os;
    }

    friend std::istream& operator>>(std::istream& is, ReliefCargo& c) {
        int catInt = 0;
        is >> c.cargoId_ >> c.itemName_ >> catInt >> c.quantity_ >> c.unitWeightKg_ >> c.triagePriority_;
        c.category_ = static_cast<SupplyCategory>(catInt);
        return is;
    }
};

// Dynamic Memory Management for Transport Manifests (Unit II: Dynamic Constructor, Copy Constructor, Destructor)
class TransportManifest {
private:
    std::string manifestId_;
    ReliefCargo* cargoItems_; // Raw dynamic array to demonstrate deep memory lifecycle
    size_t count_;
    size_t capacity_;

    void resize(size_t newCapacity) {
        ReliefCargo* newBlock = new ReliefCargo[newCapacity];
        for (size_t i = 0; i < count_; ++i) {
            newBlock[i] = cargoItems_[i];
        }
        delete[] cargoItems_;
        cargoItems_ = newBlock;
        capacity_ = newCapacity;
    }

public:
    // Dynamic Constructor
    explicit TransportManifest(std::string id = "MAN-000", size_t initialCapacity = 4)
        : manifestId_(std::move(id)),
          cargoItems_(new ReliefCargo[initialCapacity]),
          count_(0),
          capacity_(initialCapacity) {}

    // Deep Copy Constructor
    TransportManifest(const TransportManifest& other)
        : manifestId_(other.manifestId_),
          cargoItems_(new ReliefCargo[other.capacity_]),
          count_(other.count_),
          capacity_(other.capacity_) {
        for (size_t i = 0; i < count_; ++i) {
            cargoItems_[i] = other.cargoItems_[i];
        }
    }

    // Copy Assignment Operator (Strong Exception Guarantee)
    TransportManifest& operator=(const TransportManifest& other) {
        if (this == &other) return *this;
        ReliefCargo* temp = new ReliefCargo[other.capacity_];
        for (size_t i = 0; i < other.count_; ++i) {
            temp[i] = other.cargoItems_[i];
        }
        delete[] cargoItems_;
        cargoItems_ = temp;
        manifestId_ = other.manifestId_;
        count_ = other.count_;
        capacity_ = other.capacity_;
        return *this;
    }

    // Clean Destructor
    ~TransportManifest() {
        delete[] cargoItems_;
        cargoItems_ = nullptr;
    }

    void addItem(const ReliefCargo& item) {
        // Check duplicate cargo using overloaded ==
        for (size_t i = 0; i < count_; ++i) {
            if (cargoItems_[i] == item) {
                cargoItems_[i].adjustQuantity(item.getQuantity());
                return;
            }
        }
        if (count_ == capacity_) {
            resize(capacity_ * 2);
        }
        cargoItems_[count_++] = item;
    }

    double totalManifestTons() const {
        double sum = 0.0;
        for (size_t i = 0; i < count_; ++i) {
            sum += cargoItems_[i].getTotalTons();
        }
        return sum;
    }

    int maxPriorityLevel() const {
        int maxP = 0;
        for (size_t i = 0; i < count_; ++i) {
            maxP = std::max(maxP, cargoItems_[i].getPriority());
        }
        return maxP;
    }

    const std::string& getManifestId() const { return manifestId_; }
    size_t size() const { return count_; }
    const ReliefCargo& at(size_t idx) const { return cargoItems_[idx]; }

    // Operator == to verify manifest identity
    bool operator==(const TransportManifest& other) const {
        if (manifestId_ != other.manifestId_ || count_ != other.count_) return false;
        for (size_t i = 0; i < count_; ++i) {
            if (!(cargoItems_[i] == other.cargoItems_[i])) return false;
        }
        return true;
    }

    friend std::ostream& operator<<(std::ostream& os, const TransportManifest& m) {
        os << "Manifest ID: " << m.manifestId_ << " | Items: " << m.count_
           << " | Total Weight: " << std::fixed << std::setprecision(2)
           << m.totalManifestTons() << " metric tons"
           << manifestHeader;
        for (size_t i = 0; i < m.count_; ++i) {
            os << m.cargoItems_[i] << "\n";
        }
        os << manifestFooter;
        return os;
    }
};

// DistributionDepot Class with Static Logistics Analytics & Operator Overloading (Unit I & II)
class DistributionDepot {
private:
    std::string depotId_;
    std::string regionName_;
    std::unordered_map<std::string, ReliefCargo> stockLedger_; // Unit IV STL unordered_map
    bool lockdownOverride_;

    // Static Analytics (Unit II)
    static double totalDispatchedTonnage_;
    static int totalDispatchOperations_;

    friend class IncidentCommander; // Unit I: Friend access to bypass stock/lockdown restrictions

public:
    DistributionDepot(std::string id, std::string region)
        : depotId_(std::move(id)), regionName_(std::move(region)), lockdownOverride_(false) {}

    // Operator += to restock supplies at transit depots (Unit II)
    DistributionDepot& operator+=(const ReliefCargo& incoming) {
        auto it = stockLedger_.find(incoming.getId());
        if (it != stockLedger_.end()) {
            it->second.adjustQuantity(incoming.getQuantity());
        } else {
            stockLedger_[incoming.getId()] = incoming;
        }
        return *this;
    }

    // Operator -= to deduct cargo upon vehicle departure (Unit II)
    DistributionDepot& operator-=(const ReliefCargo& outgoing) {
        auto it = stockLedger_.find(outgoing.getId());
        if (it == stockLedger_.end() || it->second.getQuantity() < outgoing.getQuantity()) {
            throw ReliefException("Insufficient depot inventory for cargo ID: " + outgoing.getId());
        }
        it->second.adjustQuantity(-outgoing.getQuantity());
        double tons = outgoing.getTotalTons();
        totalDispatchedTonnage_ += tons;
        totalDispatchOperations_++;
        return *this;
    }

    // Call by Reference & Return by Reference (Unit I)
    ReliefCargo& getCargoReference(const std::string& cargoId) {
        auto it = stockLedger_.find(cargoId);
        if (it == stockLedger_.end()) {
            throw ReliefException("Cargo ID not found in depot ledger: " + cargoId);
        }
        return it->second;
    }

    // Function Overloading #1: Volume/Weight-based general supply allocation (Unit I)
    bool allocateSupply(const std::string& cargoId, double maxWeightTons,
                        TransportManifest& outManifest, int& allocatedUnits) {
        auto it = stockLedger_.find(cargoId);
        if (it == stockLedger_.end() || it->second.getQuantity() <= 0) return false;

        ReliefCargo& stock = it->second;
        int maxUnitsByWeight = static_cast<int>((maxWeightTons * 1000.0) / stock.getUnitWeightKg());
        allocatedUnits = std::min(stock.getQuantity(), maxUnitsByWeight);
        if (allocatedUnits <= 0) return false;

        ReliefCargo batch(stock.getId(), stock.getName(), stock.getCategory(),
                          allocatedUnits, stock.getUnitWeightKg(), stock.getPriority());
        *this -= batch;
        outManifest.addItem(batch);
        return true;
    }

    // Function Overloading #2: Priority Emergency Medical Triage allocation (Unit I)
    bool allocateSupply(SupplyCategory urgentCategory, int minTriagePriority, int requestedUnits,
                        TransportManifest& outManifest) {
        bool fulfilled = false;
        for (auto& pair : stockLedger_) {
            ReliefCargo& stock = pair.second;
            if (stock.getCategory() == urgentCategory &&
                stock.getPriority() >= minTriagePriority &&
                stock.getQuantity() > 0) {
                int take = std::min(stock.getQuantity(), requestedUnits);
                ReliefCargo batch(stock.getId(), stock.getName(), stock.getCategory(),
                                  take, stock.getUnitWeightKg(), stock.getPriority());
                *this -= batch;
                outManifest.addItem(batch);
                requestedUnits -= take;
                fulfilled = true;
                if (requestedUnits == 0) break;
            }
        }
        return fulfilled;
    }

    // Static Analytics Accessors
    static double getTotalDispatchedTonnage() { return totalDispatchedTonnage_; }
    static int getTotalDispatchOperations() { return totalDispatchOperations_; }

    void printInventorySummary(std::ostream& os) const {
        os << "\n[DEPOT STATUS: " << depotId_ << " - " << regionName_ << "]"
           << (lockdownOverride_ ? " [COMMANDER OVERRIDE ACTIVE]" : "")
           << manifestHeader;
        for (const auto& kv : stockLedger_) {
            os << kv.second << "\n";
        }
        os << manifestFooter;
    }
};

double DistributionDepot::totalDispatchedTonnage_ = 0.0;
int DistributionDepot::totalDispatchOperations_ = 0;

// Friend Class IncidentCommander (Unit I: Overrides access barriers during critical triage)
class IncidentCommander {
private:
    std::string commanderName_;
    std::string agencyCode_; // e.g., "NDRF-HQ"
public:
    IncidentCommander(std::string name, std::string code)
        : commanderName_(std::move(name)), agencyCode_(std::move(code)) {}

    void elevateCargoTriagePriority(ReliefCargo& cargo, int emergencyPriority) const {
        std::cout << "[COMMANDER OVERRIDE by " << commanderName_ << " (" << agencyCode_ << ")] "
                  << "Elevating " << cargo.itemName_ << " priority from "
                  << cargo.triagePriority_ << " -> " << emergencyPriority << "\n";
        cargo.triagePriority_ = emergencyPriority;
    }

    void authorizeEmergencyReserveRelease(DistributionDepot& depot, const std::string& cargoId,
                                          int emergencyUnits, TransportManifest& manifest) const {
        depot.lockdownOverride_ = true;
        auto it = depot.stockLedger_.find(cargoId);
        if (it != depot.stockLedger_.end() && it->second.quantity_ >= emergencyUnits) {
            it->second.quantity_ -= emergencyUnits;
            DistributionDepot::totalDispatchedTonnage_ +=
                calculateMetricTonnage(emergencyUnits, it->second.unitWeightKg_);
            DistributionDepot::totalDispatchOperations_++;

            ReliefCargo emergencyBatch(it->second.cargoId_, it->second.itemName_,
                                       it->second.category_, emergencyUnits,
                                       it->second.unitWeightKg_, 10);
            manifest.addItem(emergencyBatch);
            std::cout << "[COMMANDER OVERRIDE] Directly released " << emergencyUnits
                      << " units of " << it->second.itemName_ << " from depot " << depot.depotId_ << ".\n";
        }
    }
};

} // namespace Inventory
} // namespace ReliefOps

// ============================================================================
// UNIT III & IV: LOGISTICS NAMESPACE, CONTAINERSHIP, VIRTUAL INHERITANCE,
//                POLYMORPHISM, PRIORITY QUEUE & BINARY PERSISTENCE
// ============================================================================
namespace ReliefOps {
namespace Logistics {

using namespace ReliefOps::Inventory;

// Composed Component 1: GPSBeacon (Unit III Containership)
class GPSBeacon {
private:
    std::string transponderId_;
    double latitude_;
    double longitude_;
    bool satelliteLock_;
public:
    GPSBeacon(std::string id, double lat, double lon)
        : transponderId_(std::move(id)), latitude_(lat), longitude_(lon), satelliteLock_(true) {}

    void updateCoordinates(double lat, double lon) {
        latitude_ = lat;
        longitude_ = lon;
    }

    std::string getTelemetry() const {
        return "GPS[" + transponderId_ + " @ " +
               std::to_string(latitude_) + ", " + std::to_string(longitude_) + "]";
    }
};

// Composed Component 2: EmergencyLifeSupportUnit (Unit III Containership)
class EmergencyLifeSupportUnit {
private:
    bool equipped_;
    int icuBeds_;
    int oxygenCylinders_;
public:
    EmergencyLifeSupportUnit(bool equipped = false, int beds = 0, int o2 = 0)
        : equipped_(equipped), icuBeds_(beds), oxygenCylinders_(o2) {}

    bool isOperational() const { return equipped_ && oxygenCylinders_ > 0; }
    std::string statusSummary() const {
        if (!equipped_) return "LifeSupport: None";
        return "LifeSupport: ACTIVE (ICU Beds: " + std::to_string(icuBeds_) +
               ", O2 Tanks: " + std::to_string(oxygenCylinders_) + ")";
    }
};

struct CorridorConditions {
    std::string sectorName;
    double distanceKm;
    bool roadFractured;
    bool floodedZone;
    bool severeStorm;
};

// Virtual Base Class: ReliefVehicle (Unit III)
class ReliefVehicle {
protected:
    std::string vehicleId_;
    std::string agency_;
    double maxPayloadTons_;
    double fuelLiters_;
    double baseFuelRateLPerKm_;
    GPSBeacon beacon_;                        // Containership (Unit III)
    EmergencyLifeSupportUnit lifeSupport_;    // Containership (Unit III)

    static int activeFleetCount_;             // Static analytics (Unit II)

public:
    ReliefVehicle(std::string id, std::string agency, double maxPayload,
                  double fuel, double fuelRate, GPSBeacon beacon,
                  EmergencyLifeSupportUnit lifeSupport = EmergencyLifeSupportUnit())
        : vehicleId_(std::move(id)), agency_(std::move(agency)),
          maxPayloadTons_(maxPayload), fuelLiters_(fuel), baseFuelRateLPerKm_(fuelRate),
          beacon_(std::move(beacon)), lifeSupport_(std::move(lifeSupport)) {
        ++activeFleetCount_;
    }

    // Virtual Destructor for Safe Polymorphic Fleet Deallocation (Unit III)
    virtual ~ReliefVehicle() {
        --activeFleetCount_;
    }

    // Runtime Polymorphic Route Evaluation (Unit III)
    virtual void evaluateRoute(const CorridorConditions& corridor, const TransportManifest& manifest) const = 0;
    virtual std::string getVehicleType() const = 0;

    void validatePayloadAndFuel(const CorridorConditions& corridor, double cargoTons) const {
        if (cargoTons > maxPayloadTons_) {
            throw CargoOverweightException(cargoTons, maxPayloadTons_);
        }
        double neededFuel = estimateFuelConsumption(corridor.distanceKm, cargoTons, baseFuelRateLPerKm_);
        if (neededFuel > fuelLiters_) {
            double maxRange = fuelLiters_ / (baseFuelRateLPerKm_ * (1.0 + 0.04 * cargoTons));
            throw FuelDepletionException(vehicleId_, corridor.distanceKm, maxRange);
        }
    }

    const std::string& getId() const { return vehicleId_; }
    const std::string& getAgency() const { return agency_; }
    double getMaxPayload() const { return maxPayloadTons_; }
    std::string getTelemetryInfo() const {
        return beacon_.getTelemetry() + " | " + lifeSupport_.statusSummary();
    }

    static int getActiveFleetCount() { return activeFleetCount_; }
};

int ReliefVehicle::activeFleetCount_ = 0;

// Derived Class 1 with Virtual Inheritance: GroundConvoy (Unit III)
class GroundConvoy : virtual public ReliefVehicle {
protected:
    int axleCount_;
    bool allTerrainTrack_;
public:
    GroundConvoy(std::string id, std::string agency, double maxPayload,
                 double fuel, double fuelRate, GPSBeacon beacon,
                 int axles, bool allTerrain,
                 EmergencyLifeSupportUnit lifeSupport = EmergencyLifeSupportUnit())
        : ReliefVehicle(std::move(id), std::move(agency), maxPayload, fuel, fuelRate,
                        std::move(beacon), std::move(lifeSupport)),
          axleCount_(axles), allTerrainTrack_(allTerrain) {}

    std::string getVehicleType() const override { return "Heavy Ground Convoy"; }

    void evaluateRoute(const CorridorConditions& corridor, const TransportManifest& manifest) const override {
        validatePayloadAndFuel(corridor, manifest.totalManifestTons());
        if (corridor.floodedZone) {
            throw DamagedCorridorException(corridor.sectorName, "Submerged highway impassable for ground axles.");
        }
        if (corridor.roadFractured && !allTerrainTrack_) {
            throw DamagedCorridorException(corridor.sectorName, "Seismic fault rupture requires tracked all-terrain drive.");
        }
        std::cout << "[ROUTE APPROVED - GROUND] " << vehicleId_ << " (" << agency_
                  << ") cleared for " << corridor.sectorName << " (" << corridor.distanceKm << " km).\n";
    }
};

// Derived Class 2 with Virtual Inheritance: AerialTransport (Unit III)
class AerialTransport : virtual public ReliefVehicle {
protected:
    double maxAltitudeMeters_;
    bool isUnmannedQuadcopter_;
public:
    AerialTransport(std::string id, std::string agency, double maxPayload,
                    double fuel, double fuelRate, GPSBeacon beacon,
                    double maxAlt, bool isUAV,
                    EmergencyLifeSupportUnit lifeSupport = EmergencyLifeSupportUnit())
        : ReliefVehicle(std::move(id), std::move(agency), maxPayload, fuel, fuelRate,
                        std::move(beacon), std::move(lifeSupport)),
          maxAltitudeMeters_(maxAlt), isUnmannedQuadcopter_(isUAV) {}

    std::string getVehicleType() const override {
        return isUnmannedQuadcopter_ ? "SAR Quadcopter UAV" : "Emergency MedEvac Helicopter";
    }

    void evaluateRoute(const CorridorConditions& corridor, const TransportManifest& manifest) const override {
        validatePayloadAndFuel(corridor, manifest.totalManifestTons());
        if (corridor.severeStorm) {
            throw DamagedCorridorException(corridor.sectorName, "Zero-visibility cyclonic shear grounds rotary aircraft.");
        }
        std::cout << "[ROUTE APPROVED - AERIAL] " << vehicleId_ << " (" << agency_
                  << ") bypassing surface damage to " << corridor.sectorName << ".\n";
    }
};

// Derived Class 3 with Virtual Inheritance: AmphibiousCraft (Unit III)
class AmphibiousCraft : virtual public ReliefVehicle {
protected:
    double draftDepthMeters_;
    bool airCushionHover_;
public:
    AmphibiousCraft(std::string id, std::string agency, double maxPayload,
                    double fuel, double fuelRate, GPSBeacon beacon,
                    double draft, bool hover,
                    EmergencyLifeSupportUnit lifeSupport = EmergencyLifeSupportUnit())
        : ReliefVehicle(std::move(id), std::move(agency), maxPayload, fuel, fuelRate,
                        std::move(beacon), std::move(lifeSupport)),
          draftDepthMeters_(draft), airCushionHover_(hover) {}

    std::string getVehicleType() const override { return "Amphibious Relief Craft"; }

    void evaluateRoute(const CorridorConditions& corridor, const TransportManifest& manifest) const override {
        validatePayloadAndFuel(corridor, manifest.totalManifestTons());
        if (!corridor.floodedZone && !airCushionHover_) {
            throw DamagedCorridorException(corridor.sectorName, "Dry rubble corridor requires hovercraft skirt.");
        }
        std::cout << "[ROUTE APPROVED - AMPHIBIOUS] " << vehicleId_ << " (" << agency_
                  << ") navigating floodwaters in " << corridor.sectorName << ".\n";
    }
};

// Hybrid Vehicle Resolving Diamond Inheritance via Virtual Base Classes (Unit III)
class MobileMedicalAmphibian : public GroundConvoy, public AmphibiousCraft {
private:
    std::string surgicalTeamLead_;
public:
    MobileMedicalAmphibian(std::string id, std::string agency, double maxPayload,
                           double fuel, double fuelRate, GPSBeacon beacon,
                           EmergencyLifeSupportUnit lifeSupport, std::string surgeon)
        : ReliefVehicle(std::move(id), std::move(agency), maxPayload, fuel, fuelRate,
                        std::move(beacon), std::move(lifeSupport)),
          GroundConvoy(vehicleId_, agency_, maxPayload, fuel, fuelRate,
                       GPSBeacon("SHARED", 0, 0), 8, true),
          AmphibiousCraft(vehicleId_, agency_, maxPayload, fuel, fuelRate,
                          GPSBeacon("SHARED", 0, 0), 1.2, true),
          surgicalTeamLead_(std::move(surgeon)) {}

    std::string getVehicleType() const override {
        return "Hybrid Amphibious Mobile ICU Unit";
    }

    void evaluateRoute(const CorridorConditions& corridor, const TransportManifest& manifest) const override {
        validatePayloadAndFuel(corridor, manifest.totalManifestTons());
        if (!lifeSupport_.isOperational()) {
            throw DamagedCorridorException(corridor.sectorName, "Mobile ICU oxygen reserves depleted.");
        }
        std::cout << "[ROUTE APPROVED - HYBRID MED-ICU] " << vehicleId_
                  << " (Lead: " << surgicalTeamLead_ << ") traversing fractured & flooded terrain into "
                  << corridor.sectorName << " | " << lifeSupport_.statusSummary() << "\n";
    }
};

// Dispatch Order & Custom Functor for Urgent Medical Triage Sorting (Unit IV)
struct DispatchMission {
    std::string missionId;
    ReliefVehicle* vehicle;
    TransportManifest manifest;
    CorridorConditions corridor;
    int urgencyScore; // Derived from medical triage priority & sector severity
};

struct UrgentTriageComparator {
    bool operator()(const DispatchMission& a, const DispatchMission& b) const {
        if (a.urgencyScore != b.urgencyScore) {
            return a.urgencyScore < b.urgencyScore; // Max-heap: higher urgency first
        }
        return a.manifest.maxPriorityLevel() < b.manifest.maxPriorityLevel();
    }
};

// Generic Template Class: PriorityDispatchQueue<T, Comparator> (Unit IV)
template <typename T, typename Compare = std::less<T>>
class PriorityDispatchQueue {
private:
    std::priority_queue<T, std::vector<T>, Compare> pq_;
public:
    void enqueue(const T& item) { pq_.push(item); }
    void enqueue(T&& item) { pq_.push(std::move(item)); }

    T dequeue() {
        if (pq_.empty()) {
            throw ReliefException("PriorityDispatchQueue underflow: No pending dispatch missions.");
        }
        T topItem = std::move(const_cast<T&>(pq_.top()));
        pq_.pop();
        return topItem;
    }

    const T& peek() const {
        if (pq_.empty()) {
            throw ReliefException("PriorityDispatchQueue is empty.");
        }
        return pq_.top();
    }

    bool empty() const { return pq_.empty(); }
    size_t size() const { return pq_.size(); }
};

// Fixed-Layout POD Record for Safe Binary Serialization (Unit IV)
struct BinaryAuditRecord {
    char missionId[24];
    char vehicleId[24];
    char agency[24];
    char sector[32];
    double dispatchedTons;
    int itemCount;
    int urgencyScore;
};

// Atomic Binary Stream Persistence Manager (Unit IV)
class AuditPersistenceManager {
public:
    static void saveAuditLogsAtomic(const std::string& targetFile,
                                    const std::vector<BinaryAuditRecord>& records) {
        const std::string tempFile = targetFile + ".tmp";
        {
            std::ofstream out(tempFile, std::ios::binary | std::ios::trunc);
            if (!out) {
                throw ReliefException("Failed to open temporary binary audit file: " + tempFile);
            }
            size_t count = records.size();
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));
            if (count > 0) {
                out.write(reinterpret_cast<const char*>(records.data()),
                          static_cast<std::streamsize>(count * sizeof(BinaryAuditRecord)));
            }
            out.flush();
            if (!out.good()) {
                throw ReliefException("Binary stream write failure during audit serialization.");
            }
        }
        // Atomic replacement of target file
        std::remove(targetFile.c_str());
        if (std::rename(tempFile.c_str(), targetFile.c_str()) != 0) {
            throw ReliefException("Atomic rename failed for audit log: " + targetFile);
        }
    }

    static std::vector<BinaryAuditRecord> loadAuditLogs(const std::string& targetFile) {
        std::ifstream in(targetFile, std::ios::binary);
        if (!in) {
            throw ReliefException("Failed to open binary audit log for reading: " + targetFile);
        }
        size_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        std::vector<BinaryAuditRecord> records(count);
        if (count > 0) {
            in.read(reinterpret_cast<char*>(records.data()),
                    static_cast<std::streamsize>(count * sizeof(BinaryAuditRecord)));
        }
        return records;
    }
};

} // namespace Logistics
} // namespace ReliefOps

// ============================================================================
// INTEGRATED SYSTEM SIMULATION (DEMONSTRATING ALL UNITS I–IV)
// ============================================================================
int main() {
    using namespace ReliefOps;
    using namespace ReliefOps::Inventory;
    using namespace ReliefOps::Logistics;

    std::cout << "=== POST-EARTHQUAKE MULTI-AGENCY EMERGENCY RELIEF LOGISTICS HUB (ERL-HUB) ===\n";

    try {
        // 1. Initialize Regional Distribution Depot & Stock via Overloaded += (Unit II)
        DistributionDepot hubAlpha("HUB-NORTH-01", "Himalayan Foothill Epicenter Zone");

        hubAlpha += ReliefCargo("CRG-101", "MRE_Ration_Packs", SupplyCategory::RATION_KIT, 500, 12.0, 6);
        hubAlpha += ReliefCargo("CRG-202", "Trauma_Surgical_Kits", SupplyCategory::TRAUMA_MEDICINE, 200, 5.0, 9);
        hubAlpha += ReliefCargo("CRG-303", "Purified_Water_Bladders", SupplyCategory::CLEAN_WATER, 150, 50.0, 8);
        hubAlpha += ReliefCargo("CRG-404", "Thermal_Foil_Blankets", SupplyCategory::THERMAL_BLANKET, 800, 1.5, 5);

        hubAlpha.printInventorySummary(std::cout);

        // 2. Friend Class IncidentCommander Override & Reference Update (Unit I)
        IncidentCommander ndrfCmdr("Col. Vikram Rathore", "NDRF-07");
        ReliefCargo& blanketRef = hubAlpha.getCargoReference("CRG-404"); // Return by reference
        ndrfCmdr.elevateCargoTriagePriority(blanketRef, 9); // Hypothermia alert override

        // 3. Build Transport Manifests using Overloaded allocateSupply() (Unit I & II)
        TransportManifest medManifest("MAN-MED-901");
        hubAlpha.allocateSupply(SupplyCategory::TRAUMA_MEDICINE, 8, 80, medManifest);
        ndrfCmdr.authorizeEmergencyReserveRelease(hubAlpha, "CRG-404", 150, medManifest);

        TransportManifest bulkManifest("MAN-BLK-902");
        int allocatedRations = 0;
        hubAlpha.allocateSupply("CRG-101", 3.6, bulkManifest, allocatedRations); // 300 units = 3.6t

        std::cout << "\nGenerated Priority Medical Manifest:\n" << medManifest;

        // 4. Polymorphic Vehicle Fleet & Diamond Inheritance Hybrid Unit (Unit III)
        std::vector<ReliefVehicle*> fleet;
        fleet.push_back(new AerialTransport(
            "HELI-MED-01", "IAF-MedEvac", 2.5, 900.0, 1.8,
            GPSBeacon("SAT-H01", 30.7333, 76.7794), 4500.0, false,
            EmergencyLifeSupportUnit(true, 2, 6)
        ));
        fleet.push_back(new GroundConvoy(
            "TRK-NDRF-04", "NDRF-Logistics", 8.0, 600.0, 0.45,
            GPSBeacon("SAT-G04", 30.7410, 76.7820), 6, true
        ));
        fleet.push_back(new MobileMedicalAmphibian(
            "AMPH-ICU-09", "Army-MedCorps", 5.0, 750.0, 0.65,
            GPSBeacon("SAT-A09", 30.7502, 76.7911),
            EmergencyLifeSupportUnit(true, 4, 12), "Maj. Dr. Ananya Sharma"
        ));

        std::cout << "\nActive Polymorphic Fleet Count: " << ReliefVehicle::getActiveFleetCount() << "\n";

        // 5. Enqueue Missions into Template PriorityDispatchQueue with Custom Functor (Unit IV)
        PriorityDispatchQueue<DispatchMission, UrgentTriageComparator> dispatchQueue;

        dispatchQueue.enqueue(DispatchMission{
            "MSN-001", fleet[1], bulkManifest,
            CorridorConditions{"Valley-Highway-B", 85.0, true, false, false}, 7
        });

        dispatchQueue.enqueue(DispatchMission{
            "MSN-002", fleet[2], medManifest,
            CorridorConditions{"Dam-Breach-Sector-4", 62.0, true, true, false}, 10
        });

        // 6. Execute Highest-Priority Missions & Record Binary Audit Trail (Unit III & IV)
        std::vector<BinaryAuditRecord> auditRecords;

        std::cout << "\n--- EXECUTING PRIORITY DISPATCH QUEUE ---\n";
        while (!dispatchQueue.empty()) {
            DispatchMission current = dispatchQueue.dequeue();
            std::cout << "Evaluating Mission [" << current.missionId << "] | Vehicle: "
                      << current.vehicle->getVehicleType() << "\n"
                      << "  Telemetry: " << current.vehicle->getTelemetryInfo() << "\n";

            // Dynamic polymorphism: virtual evaluateRoute()
            current.vehicle->evaluateRoute(current.corridor, current.manifest);

            BinaryAuditRecord rec{};
            std::strncpy(rec.missionId, current.missionId.c_str(), sizeof(rec.missionId) - 1);
            std::strncpy(rec.vehicleId, current.vehicle->getId().c_str(), sizeof(rec.vehicleId) - 1);
            std::strncpy(rec.agency, current.vehicle->getAgency().c_str(), sizeof(rec.agency) - 1);
            std::strncpy(rec.sector, current.corridor.sectorName.c_str(), sizeof(rec.sector) - 1);
            rec.dispatchedTons = current.manifest.totalManifestTons();
            rec.itemCount = static_cast<int>(current.manifest.size());
            rec.urgencyScore = current.urgencyScore;
            auditRecords.push_back(rec);
        }

        // 7. Demonstrate Exception Handling on Damaged Corridor / Overweight (Unit IV)
        std::cout << "\n--- TESTING EXCEPTION SAFETY GUARDRAILS ---\n";
        try {
            CorridorConditions floodedSector{"River-Basin-7", 40.0, false, true, false};
            fleet[1]->evaluateRoute(floodedSector, bulkManifest); // GroundConvoy in flooded zone
        } catch (const DamagedCorridorException& ex) {
            std::cerr << "[CAUGHT EXPECTED EXCEPTION] " << ex.what() << "\n";
        }

        // 8. Atomic Binary Persistence & Verification Readback (Unit IV)
        const std::string auditFile = "erl_hub_audit.bin";
        AuditPersistenceManager::saveAuditLogsAtomic(auditFile, auditRecords);
        auto restoredLogs = AuditPersistenceManager::loadAuditLogs(auditFile);

        std::cout << "\n--- VERIFIED BINARY AUDIT RECORDS (" << restoredLogs.size() << " entries) ---\n";
        for (const auto& r : restoredLogs) {
            std::cout << "Mission: " << r.missionId << " | Vehicle: " << r.vehicleId
                      << " (" << r.agency << ") | Sector: " << r.sector
                      << " | Tonnage: " << std::fixed << std::setprecision(2) << r.dispatchedTons
                      << "t | Urgency: " << r.urgencyScore << "\n";
        }

        // 9. Static Analytics & Virtual Destructor Fleet Cleanup (Unit II & III)
        std::cout << "\n--- FINAL HUB LOGISTICS ANALYTICS ---\n"
                  << "Total Dispatched Tonnage : " << DistributionDepot::getTotalDispatchedTonnage() << " metric tons\n"
                  << "Total Dispatch Operations: " << DistributionDepot::getTotalDispatchOperations() << "\n";

        for (ReliefVehicle* v : fleet) {
            delete v; // Invokes virtual ~ReliefVehicle() safely
        }
        fleet.clear();
        std::cout << "Post-Deallocation Active Fleet Count: " << ReliefVehicle::getActiveFleetCount() << "\n";

    } catch (const ReliefException& e) {
        std::cerr << "[CRITICAL SYSTEM ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
