#include <iostream>
#include <conio.h>
#include <Windows.h>
#include <random>
#include <iomanip>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4

using namespace std;

struct ClientRecord {
    DWORD threadId; // Идентификатор потока
    DWORD arriveTick; // Время прихода посетителя
    DWORD startTick; // Время начала обслуживания
    DWORD endTick; // Время завершения обслуживания
    BOOL served; // Был ли обслужен
    BOOL timeout; // Ушел ли по таймауту
};

struct ClubState {
    ClientRecord* clients[MAX_CLIENTS]; // Информация о посетителях
    LONG currentVisitors; // Текущее число занятых мест
    LONG maxVisitors; // Максимум одновременно занятых мест
    LONG servedCount; // Количество обслуженных посетителей
    LONG timeoutCount; // Количество ушедших по таймауту
};

ClubState club;

void Client(LPVOID param) {
    random_device rnd;
    mt19937 gen(rnd());
    uniform_int_distribution<> dist(2, 5);

    DWORD dwWaitResult;
    HANDLE semaphore = OpenSemaphore(EVENT_ALL_ACCESS, FALSE, L"SemaphoreClub");

    ClientRecord* client = (ClientRecord*)param;

    club.clients[client->threadId - 1] = client;
    client->arriveTick = GetTickCount64(); // фиксирует момент прихода

    dwWaitResult = WaitForSingleObject(semaphore, 3000); // пытается получить доступ к рабочему месту через семафор, то есть уменьшает количество свободных мест

    switch (dwWaitResult) {
    case WAIT_OBJECT_0: // если свободное место есть
        client->served = true;
        club.currentVisitors++; // занимает его
        
        client->startTick = GetTickCount64(); // фиксирует начало обслуживания
        
        Sleep(dist(gen) * 1000); // работает за компьютером случайное время
        
        ReleaseSemaphore(semaphore, 1, NULL); // увеличивает количество свободных мест
        club.currentVisitors--; // освобождает место
        
        client->endTick = GetTickCount64(); // фиксирует завершение обслуживания
        client->timeout = false;
        
        club.servedCount++;
        break;
    case WAIT_TIMEOUT: // если мест нет
        client->served = false; // если время ожидания истекло, фиксирует отказ от обслуживания
        client->timeout = true;

        club.timeoutCount++;
        break;
    }
}

void Watcher() {
    while (club.servedCount + club.timeoutCount < MAX_CLIENTS) {
        if (club.maxVisitors < club.currentVisitors) {
            club.maxVisitors = club.currentVisitors;
        }

        cout << "Занятых мест: " << club.currentVisitors << endl;
        cout << "Количество обслуженных посетителей: " << club.servedCount << endl;
        cout << "Количество посетителей ушедших по таймауту: " << club.timeoutCount << endl;
        Sleep(500);
    }

    // Среднее время
    long tick = 0, tickArrive = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i]->served) {
            tickArrive += club.clients[i]->startTick - club.clients[i]->arriveTick;
            tick += club.clients[i]->endTick - club.clients[i]->startTick;
        }
        else if (club.clients[i]->timeout) {
            tickArrive += 3;
        }
    }

    long argTickArrive = (double)tick / MAX_CLIENTS;
    long argTick = (double)tick / club.servedCount;

    // Вывод
    cout << endl;
    cout << "Количество обслуженных посетителей: " << club.servedCount << endl;
    cout << "Количество посетителей ушедших по таймауту: " << club.timeoutCount << endl;
    cout << "Среднее время ожидания посетителей: " << setprecision(2) << argTickArrive / 1000.0 << " сек" << endl;
    cout << "Среднее время обслуживания: " << setprecision(2) << argTick / 1000.0 << " сек" << endl;
    cout << "Максимальное число одновременно занятых мест: " << club.maxVisitors << endl;
    cout << "Не дождались свободного места: ";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i]->timeout)
            cout << club.clients[i]->threadId << ", ";
    }
}

int main()
{
    setlocale(0, "rus");

    HANDLE semaphore, wThread, hThread[MAX_CLIENTS];
    DWORD IdWThread, IdThread;

    ClientRecord clients[MAX_CLIENTS]{MAX_CLIENTS};
                                    //количество свободных мест 
    semaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, L"SemaphoreClub");
    if (semaphore == NULL) {
        cout << GetLastError() << endl;
        return 1;
    }

    wThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Watcher, NULL, 0, &IdWThread);
    if (wThread == NULL) {
        cout << GetLastError() << endl;
        return 1;
    }

    SetThreadPriority(wThread, THREAD_PRIORITY_LOWEST);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].threadId = i + 1;
        hThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Client, (LPVOID)&clients[i], 0, &IdThread);
        if (hThread[i] == NULL) {
            cout << GetLastError() << endl;
            return 1;
        }
        if (i <= 7)
            SetThreadPriority(hThread[i], THREAD_PRIORITY_NORMAL);
        else if (i > 7 && i <= 15)
            SetThreadPriority(hThread[i], THREAD_PRIORITY_BELOW_NORMAL);
        else if (i > 15)
            SetThreadPriority(hThread[i], THREAD_PRIORITY_HIGHEST);
    }

    WaitForMultipleObjects(MAX_CLIENTS, hThread, TRUE, INFINITE);

    for (int i = 0; i < MAX_CLIENTS; i++)
        CloseHandle(hThread[i]);

    WaitForSingleObject(wThread, INFINITE);

    CloseHandle(wThread);

    CloseHandle(semaphore);

    return 0;
}
