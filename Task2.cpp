#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <condition_variable>
#include <random>
#include <chrono>
#include <memory>
#include <algorithm>

using namespace std;

enum class VehicleType {
    CAR,
    EMERGENCY 
};

struct Vehicle {
    int id;
    VehicleType type;
    chrono::system_clock::time_point arrivalTime;
    
    Vehicle(int id, VehicleType type) : id(id), type(type), arrivalTime(chrono::system_clock::now()) {}
};

class Intersection {
private:
    int id;
    queue<Vehicle> northQueue;
    queue<Vehicle> southQueue;
    queue<Vehicle> eastQueue;
    queue<Vehicle> westQueue;
    
    mutex queueMutex;
    atomic<bool> emergencyFlag{false};
    atomic<bool> congestionFlag{false};
    atomic<bool> stopFlag{false};
    
    thread trafficLightThread;
    condition_variable cv;
    
    enum class LightState { NORTH_SOUTH, EAST_WEST, EMERGENCY, CONGESTION };
    LightState currentState = LightState::NORTH_SOUTH;
    
    chrono::seconds northSouthTime{10};
    chrono::seconds eastWestTime{10};
    
    void runTrafficLight() {
        while (!stopFlag) {
            unique_lock<mutex> lock(queueMutex);
            
            checkCongestion();
            
            if (emergencyFlag) {
                currentState = LightState::EMERGENCY;
                handleEmergency();
                emergencyFlag = false;
                continue;
            }
            
            if (congestionFlag) {
                currentState = LightState::CONGESTION;
                handleCongestion();
                congestionFlag = false;
                continue;
            }
            
            if (getTotalVehicles() > 7) {
                adaptTimings();
            }
            
            switch (currentState) {
                case LightState::NORTH_SOUTH:
                    processDirection(northQueue, southQueue, "Север-Юг");
                    currentState = LightState::EAST_WEST;
                    break;
                case LightState::EAST_WEST:
                    processDirection(eastQueue, westQueue, "Восток-Запад");
                    currentState = LightState::NORTH_SOUTH;
                    break;
                default:
                    break;
            }
            
            lock.unlock();
            this_thread::sleep_for(currentState == LightState::NORTH_SOUTH ? 
                                 northSouthTime : eastWestTime);
        }
    }
    
    void processDirection(queue<Vehicle>& primary, queue<Vehicle>& secondary, const string& dirName) {
        cout << "Перекресток " << id << ": " << dirName << " зеленый" << endl;
        
        processEmergencyVehicles(primary);
        processEmergencyVehicles(secondary);
        
        if (!primary.empty()) {
            auto vehicle = primary.front();
            primary.pop();
            cout << "Перекресток " << id << ": ТС " << vehicle.id 
                 << " проехало в направлении " << dirName << endl;
        }
        
        if (!secondary.empty()) {
            auto vehicle = secondary.front();
            secondary.pop();
            cout << "Перекресток " << id << ": ТС " << vehicle.id 
                 << " проехало в направлении " << dirName << endl;
        }
    }
    
    void processEmergencyVehicles(queue<Vehicle>& q) {
        queue<Vehicle> temp;
        while (!q.empty()) {
            auto vehicle = q.front();
            q.pop();
            if (vehicle.type == VehicleType::EMERGENCY) {
                cout << "ПРИОРИТЕТ: Перекресток " << id << ": Экстренное ТС " 
                     << vehicle.id << " проехало" << endl;
            } else {
                temp.push(vehicle);
            }
        }
        q = temp;
    }
    
    void handleEmergency() {
        cout << "Перекресток " << id << ": Активирован режим экстренной службы" << endl;
        
        processEmergencyVehicles(northQueue);
        processEmergencyVehicles(southQueue);
        processEmergencyVehicles(eastQueue);
        processEmergencyVehicles(westQueue);
        this_thread::sleep_for(chrono::seconds(5));
    }
    
    void handleCongestion() {
        cout << "Перекресток " << id << ": Активирована система управления затором" << endl;
        
        for (int i = 0; i < 2; ++i) {
            processDirection(northQueue, southQueue, "Север-Юг (аварийный режим)");
            processDirection(eastQueue, westQueue, "Восток-Запад (аварийный режим)");
        }
    }
    
    void checkCongestion() {
        size_t total = getTotalVehicles();
        if (total > 10) { 
            congestionFlag = true;
        }
    }
   
    void adaptTimings() {
        size_t ns = northQueue.size() + southQueue.size();
        size_t ew = eastQueue.size() + westQueue.size();
        
        if (ns > ew * 2) {
            northSouthTime = chrono::seconds(15);
            eastWestTime = chrono::seconds(5);
        } else if (ew > ns * 2) {
            northSouthTime = chrono::seconds(5);
            eastWestTime = chrono::seconds(15);
        } else {
            northSouthTime = chrono::seconds(10);
            eastWestTime = chrono::seconds(10);
        }
        
        cout << "Перекресток " << id << ": Адаптированы интервалы - С-Ю: " 
             << northSouthTime.count() << "с, В-З: " << eastWestTime.count() << "с" << endl;
    }
    
    size_t getTotalVehicles() {
        return northQueue.size() + southQueue.size() + eastQueue.size() + westQueue.size();
    }

public:
    Intersection(int id) : id(id) {
        trafficLightThread = thread(&Intersection::runTrafficLight, this);
    }
    
    ~Intersection() {
        stopFlag = true;
        if (trafficLightThread.joinable()) {
            trafficLightThread.join();
        }
    }
    
    void addVehicle(VehicleType type, const string& direction) {
        static atomic<int> vehicleId(1);
        lock_guard<mutex> lock(queueMutex);
        
        Vehicle vehicle(vehicleId++, type);
        
        if (direction == "north") northQueue.push(vehicle);
        else if (direction == "south") southQueue.push(vehicle);
        else if (direction == "east") eastQueue.push(vehicle);
        else if (direction == "west") westQueue.push(vehicle);
        
        cout << "Перекресток " << id << ": ТС " << vehicle.id 
             << " (" << (type == VehicleType::EMERGENCY ? "Экстренное" : "Обычное")
             << ") прибыло с направления " << direction << endl;
        
        if (type == VehicleType::EMERGENCY) {
            emergencyFlag = true;
        }
        
        cv.notify_one();
    }
    
    void printStatus() {
        lock_guard<mutex> lock(queueMutex);
        cout << "\nСтатус перекрестка " << id << ":" << endl;
        cout << "Север: " << northQueue.size() << " ТС" << endl;
        cout << "Юг: " << southQueue.size() << " ТС" << endl;
        cout << "Восток: " << eastQueue.size() << " ТС" << endl;
        cout << "Запад: " << westQueue.size() << " ТС" << endl;
        cout << "Всего: " << getTotalVehicles() << " ТС" << endl;
        cout << "Текущий режим: ";
        switch (currentState) {
            case LightState::NORTH_SOUTH: cout << "Север-Юг"; break;
            case LightState::EAST_WEST: cout << "Восток-Запад"; break;
            case LightState::EMERGENCY: cout << "Экстренный"; break;
            case LightState::CONGESTION: cout << "Аварийный"; break;
        }
        cout << endl << endl;
    }
};

int main() {
    const int INTERSECTIONS_COUNT = 10;
    vector<unique_ptr<Intersection>> intersections;
    
    for (int i = 1; i <= INTERSECTIONS_COUNT; ++i) {
        intersections.push_back(make_unique<Intersection>(i));
    }
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> intersectionDist(0, INTERSECTIONS_COUNT-1);
    uniform_int_distribution<> directionDist(0, 3);
    uniform_int_distribution<> typeDist(0, 20); 
    
    const vector<string> directions = {"north", "south", "east", "west"};
    
    for (int i = 0; i < 100; ++i) {
        int intersectionIdx = intersectionDist(gen);
        int directionIdx = directionDist(gen);
        VehicleType type = typeDist(gen) == 0 ? VehicleType::EMERGENCY : VehicleType::CAR;
        
        intersections[intersectionIdx]->addVehicle(type, directions[directionIdx]);
        
        if (i % 20 == 0) {
            for (auto& intersection : intersections) {
                intersection->printStatus();
            }
        }
        
        this_thread::sleep_for(chrono::milliseconds(200 + rand() % 300));
    }

    this_thread::sleep_for(chrono::seconds(5));

    for (auto& intersection : intersections) {
        intersection->printStatus();
    }
    
    return 0;
}
