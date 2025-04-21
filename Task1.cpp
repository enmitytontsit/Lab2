#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <random>
#include <chrono>
#include <memory>

using namespace std;

class Task {
public:
    int id;
    int priority;
    
    Task(int id, int priority) : id(id), priority(priority) {}
    
    bool operator<(const Task& other) const {
        return priority > other.priority;
    }
};


class Server {
private:
    int id;
    priority_queue<Task> taskQueue;
    mutex mtx;
    atomic<int> currentLoad{0};
    atomic<bool> stopFlag{false};
    thread workerThread;
    
    void processTasks() {
        while (!stopFlag) {
            unique_lock<mutex> lock(mtx);
            if (!taskQueue.empty()) {
                Task task = taskQueue.top();
                taskQueue.pop();
                currentLoad--;
                lock.unlock();
                
                cout << "Сервер " << id << " обрабатывает задачу " << task.id 
                     << " с приоритетом " << task.priority << endl;
                
                this_thread::sleep_for(chrono::milliseconds(100 + rand() % 200));
            } else {
                lock.unlock();
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }
    
public:
    Server(int serverId) : id(serverId) {
        workerThread = thread(&Server::processTasks, this);
    }
    
    ~Server() {
        stopFlag = true;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
    
    void addTask(Task task) {
        lock_guard<mutex> lock(mtx);
        taskQueue.push(task);
        currentLoad++;
    }
    
    int getCurrentLoad() const {
        return currentLoad;
    }
    
    int getId() const {
        return id;
    }
};

class Cluster {
private:
    vector<unique_ptr<Server>> servers;
    mutex mtx;
    const int initialServerCount = 5;
    const double loadThreshold = 0.8;
    
    void checkLoad() {
        lock_guard<mutex> lock(mtx);
        
        int totalLoad = 0;
        for (const auto& server : servers) {
            totalLoad += server->getCurrentLoad();
        }
        
        double avgLoad = static_cast<double>(totalLoad) / servers.size();
        
        if (avgLoad > loadThreshold * 4) { 
            cout << "Нагрузка превышает 80%. Добавляем новый сервер" << endl;
            addServer();
        }
    }
    
    void addServer() {
        int newId = servers.size() + 1;
        servers.push_back(make_unique<Server>(newId));
        cout << "Добавлен новый сервер " << newId << endl;
    }
    
public:
    Cluster() {
        for (int i = 1; i <= initialServerCount; ++i) {
            servers.push_back(make_unique<Server>(i));
        }
    }
    
    ~Cluster() {
    }
    
    void addTask(Task task) {
        int minLoad = numeric_limits<int>::max();
        Server* targetServer = nullptr;
        
        for (const auto& server : servers) {
            int load = server->getCurrentLoad();
            if (load < minLoad) {
                minLoad = load;
                targetServer = server.get();
            }
        }
        
        if (targetServer) {
            targetServer->addTask(task);
            
            if (servers.size() == initialServerCount && 
                static_cast<double>(minLoad + 1) > loadThreshold * 4) {
                checkLoad();
            }
        }
    }
    
    void printStatus() {
        cout << "\nТекущее состояние кластера:" << endl;
        for (const auto& server : servers) {
            cout << "Сервер " << server->getId() << ": нагрузка " 
                 << server->getCurrentLoad() << " задач" << endl;
        }
        cout << endl;
    }
};

int main() {
    srand(time(nullptr));
    Cluster cluster;
    
    for (int i = 1; i <= 30; ++i) {
        int priority = 1 + rand() % 5;
        Task task(i, priority);
        cluster.addTask(task);
        
        if (i % 5 == 0) {
            cluster.printStatus();
        }
        
        this_thread::sleep_for(chrono::milliseconds(200 + rand() % 300));
    }
    
    this_thread::sleep_for(chrono::seconds(2));
    cluster.printStatus();
    
    return 0;
}
