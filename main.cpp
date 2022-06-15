#include <iostream>
#include <thread>
#include <Windows.h>
#include <mutex>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <bits/stdc++.h>
#include <random>
#include <algorithm> // for copy
#include <iterator> // for ostream_iterator


using namespace std;
//инициализируем переменную с флагом остановки работы, чтобы останавливаться по Ctrl C
bool stop = false;
//обработчик Ctrl C
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT:
        cout << "SIGINT signal received!" << endl;
        cout << "Stopping generator and all devices" << endl;
        cout.flush();
        stop = true;
        return TRUE;
    default:
        return FALSE;
    }
}

struct Request {
    int requestClass, type;
};

unsigned int RandomBetween(const int nMin, const int nMax)
{
    std::random_device seeder;
    //then make a mersenne twister engine
    std::mt19937 engine(seeder());
    //then the easy part... the distribution
    std::uniform_int_distribution<int> dist(nMin, nMax);
    //then just generate the integer like this:
    return dist(engine);
}

void clear_screen (void)
{
    DWORD n;                         /* Number of characters written */
    DWORD size;                      /* number of visible characters */
    COORD coord = {0};               /* Top left screen position */
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    /* Get a handle to the console */
    HANDLE h = GetStdHandle ( STD_OUTPUT_HANDLE );

    GetConsoleScreenBufferInfo ( h, &csbi );

    /* Find the number of characters to overwrite */
    size = csbi.dwSize.X * csbi.dwSize.Y;

    /* Overwrite the screen buffer with whitespace */
    FillConsoleOutputCharacter ( h, TEXT ( ' ' ), size, coord, &n );
    GetConsoleScreenBufferInfo ( h, &csbi );
    FillConsoleOutputAttribute ( h, csbi.wAttributes, size, coord, &n );

    /* Reset the cursor to the top left position */
    SetConsoleCursorPosition ( h, coord );
}

void printHelp(const std::string& name = ""){
    //печатаем информацию как пользоваться программой
    string help = "Usage: \n"
                  "\t" + name + " queue_length groups_num group_capacity\n"
                                "EVERY argument is required\n\n"
                                "\tqueue_length > 0\n"
                                "\tgroups_num, group_capacity >= 2\n"
                                "add \"debug\" flag to print queue\n";
    cout << help << endl;
}

void Enqueue(vector<Request> &queue, Request &request) {
    if (queue.empty()) //если очередь пуста, просто добавляем в вектор заявку
        queue.push_back(request);
    else { //иначе ищем линейным поиском, где начинается подмножество заявок текущего типа
        for (int index = 0; index < queue.size(); index++){
            if (queue[index].type < request.type) {
                queue.insert(queue.begin() + index, request); //и вставляем заявку в его начало
                break;
            }
        }
    }
}


void Device(int groupNumber, int threadId, vector<Request> &queue, mutex &queueMutex, vector<string> &status) {
    bool isBusy = false; //сразу после создания прибор ещё не выполняет заявку
    while (!stop) { //пока не нажали Ctrl C работаем
        isBusy = false;
        int type;
        string threadStatus;
        queueMutex.lock(); //блокируем очередь на время поиска заявок
        if (!queue.empty()) { //и проверяем не пуста ли очередь
            for (int i = 0; i < queue.size() && !isBusy; i++) { //последовательный поиск по очереди заявок
                if (queue[i].requestClass == groupNumber) { // если нашлась заявка нужного типа - берём её и работаем
                    type = queue[i].type; //для отчёта о статусе сохраняем тип (приоритет) найденной заявки
                    queue.erase(queue.begin() + i); //удаляем заявку из очереди
                    isBusy = true; //теперь прибор выполняет заявку
                }
            }
        }
        queueMutex.unlock(); //разблокируем очередь
        //отчитываемся о статусе
        threadStatus = "Device thread " + to_string(threadId);
        threadStatus += isBusy ? " is working on task of type " + to_string(type) : " is vacant"; //тернарка
        status[threadId] = threadStatus; //обновили статус прибора в векторе
        this_thread::sleep_for(chrono::milliseconds(RandomBetween(64, 1024))); // поток прибора засыпает на случайное время
        // После окончания этого времени, прибор считается свободным (в начале цикла)
    }
}

Request prepareRequest(int groupsCount){
    //функция подготавливает заявки случайных типов для случайных классов
    return Request {
        .requestClass = static_cast<int>(RandomBetween(0, groupsCount - 1)),
        .type = static_cast<int>(RandomBetween(1, 3)),
    };
}

void generator(int groupsCount, vector<Request> &queue, int &capacity, mutex &queueMutex, vector<string> &status) {
    while (!stop) {
        string generatorStatus;
        queueMutex.lock();//блокируем очередь на время проверок и добавления заявок
        if (queue.size() < capacity) { //Генератор через случайные промежутки времени добавляет в очередь заявки из разных групп со случайным типов
            Request request = prepareRequest(groupsCount); //подготовка заявки
            Enqueue(queue, request); //подготовленную заявку помещаем в очередь
            generatorStatus = to_string(queue.size()) + " request(s) in queue";
        } // Генератор неактивен, если в очереди нет свободных мест
        queueMutex.unlock(); //разблокируем очередь
        status[status.size() - 1] = generatorStatus; //обновляем статус генератора в последнем элементе вектора со статусами
        this_thread::sleep_for(chrono::milliseconds(RandomBetween(16, 128)));

    }
}

void printQueue(mutex &queueMutex, const vector<Request> &queue) {
    cout << "\nQueue content:" << endl;
    queueMutex.lock(); //если этого не делать, будет гонка и будем читать освобождённую память
    for(vector<int>::size_type i = 0; i != queue.size(); i++) {
        cout << "Request " << i << " is type: " << queue[i].type << "; class: " << queue[i].requestClass << endl;
    }
    cout.flush();
    queueMutex.unlock();
}

void printStatus(int groupsCount, int deviceCount, const vector<string> &status) {
    for (int i = 0; i < groupsCount * deviceCount + 1; i++) {
        cout << status[i] << endl;
    }
    cout.flush();
}

int main(int argc, char *argv[]) {
    srand(time(0));
    setlocale(LC_ALL, "rus");
    int capacity, groupsCount, deviceCount;
    //считываем из аргументов параметры для программы
    capacity = argc > 1 ? atoi(argv[1]) : -1;
    groupsCount = argc > 2 ? atoi(argv[2]) : -1;
    deviceCount = argc > 3 ? atoi(argv[3]) : -1;
    bool DEBUG = argc > 4 && std::string(argv[4]) == "debug";
    //если что-то ввели не так, или не ввели, выводим сообщение и выходим
    if (capacity <= 0 || groupsCount < 2 || deviceCount < 2){
        printHelp("MultiThreading.exe");
        return 1;
    }

    mutex queueMutex; //мьютекс
    vector<Request> queue; // очередь-"накопитель"
    vector<string> status(groupsCount * deviceCount + 1, ""); //тексты статусов потоков
    //регистрируем обработчик Ctrl C, если не удалось - выходим
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        return -1;
    }
    // создаём вектор для потоков устройств
    vector<thread> deviceThread(groupsCount * deviceCount);
    for (int i = 0; i < groupsCount * deviceCount; i++) {
        //и создаём потоки устройств
        deviceThread[i] = thread(Device, (int) i / deviceCount, i, ref(queue), ref(queueMutex), ref(status));
    }
    //создаём генератор
    thread gen(generator, ref(groupsCount), ref(queue), ref(capacity), ref(queueMutex), ref(status));
    //пока не нажали Ctrl C, в цикле выводим текущий статус
    while (!stop) {
        clear_screen();
        printStatus(groupsCount, deviceCount, status);
        if (DEBUG) printQueue(queueMutex, queue);
        this_thread::sleep_for(chrono::milliseconds(200));
    }
    //если попали сюда, значит, было нажато Ctrl C. Останавливаем генератор и потоки
    gen.join();
    cout << "Generator thread stopped" << endl;
    for (int i = 0; i < groupsCount * deviceCount; i++) {
        deviceThread[i].join();
        cout << "Device " << i << " thread stopped" << endl;
    }
    return 0;
}
