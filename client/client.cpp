#include <bits/stdc++.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <semaphore.h>
using namespace std;

vector<string> commandParser(string);

struct thread_arg{
    string data1;
    int data2;
};

struct thread_arg_downloadChunkThread{
    long startingIndex;
    string fileHash;
    vector<vector<string>> chunkWiseSeederList;
};

struct arg_struct {
    int port;
    string data;
};

struct userDetails {
    bool isLoggedIn = false;
    string userId;
};

struct comp{
    bool operator()(vector<string> a, vector<string> b){
        return a.size() < b.size();
    }
};

struct fileMetaData {
    string fileName;
    string filePath;
    unsigned long long fileSize;
    string groupId;
    long long noOfChunks;
    vector<int> bitmap;
    unordered_map<string, pair<string, string>> seederInfo;       //{userId, {ip, port}}
};

//Global variables
long chunkSize = 524288;
long miniChunkSize = 16384;
string thisIp;
int thisPort;
string trackerIp;
int trackerPort;

struct userDetails *currUser = new userDetails;

//Data structures
unordered_map<string, struct fileMetaData *> downloadRequest, downloadInProgress, downloadCompleted;       //{SHA, fileMetadata}

//utility function
string convertToString(char* a)
{
    long i,counter=0;
    while(a[counter]!='\0'){
        counter++;
    }
    string s = "";
    for (i = 0; i < counter; i++) {
        s = s + a[i];
    }
    return s;
}

string calFileName(string path){
    long n = path.size();
    string filename = "";
    for(long i = n - 1; i >= 0; i--){
        if(path[i] == '/'){
            break;
        }
        filename = path[i] + filename;
    }
    return filename;
}

void calFileSize(double fileSize){
    long GB = 1000 * 1000 * 1000;
    long MB = 1000 * 1000;
    long KB = 1000;
    double result;

    if (fileSize >= GB) {
        result = fileSize / GB;
        cout << fixed << setprecision(1) << result << " GB\n";
        
    }
    else if (fileSize >= MB) {
        result = fileSize / MB;
        cout << fixed << setprecision(1) << result << " MB\n";
    }
    else if (fileSize >= KB){
        result = fileSize / KB;
        cout << fixed << setprecision(1) << result << " KB\n";
    }
    else {
        cout << fileSize << " B\n";
    }
}

string calFileHash(string path){
	long readCount;
	char buff[524288],sha[SHA_DIGEST_LENGTH];
	int fd = open(path.c_str(), O_RDONLY);
	if(fd<0){
		return "-1";
	}
	else{
		SHA_CTX ctx;
    	SHA1_Init(&ctx);
		    while ((readCount = read(fd, buff, 524288)) > 0){
				SHA1_Update(&ctx,buff,readCount);
			}
		SHA1_Final((unsigned char*)sha, &ctx);
		string hash = "";
		for(long i = 0; i < 20; i++){
			if(sha[i] != '\0' && sha[i] != '\n'){
				hash += sha[i];
			}
			else{
				hash += ' ';
			}
		}
		return hash;
	}
}

string chunkSHA(char *chunkBuffer, long readCount){
    unsigned char hash[SHA_DIGEST_LENGTH];
    // SHA1((const unsigned char *)chunkBuffer, sizeof(chunkBuffer) - 1, hash);
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, chunkBuffer, readCount);
    SHA1_Final((unsigned char*)hash,&ctx);

    string calSHA = "";
		for(long i = 0; i < 20; i++){
			if(hash[i] != '\0'){
				calSHA += hash[i];
			}
			else{
				calSHA += ' ';
			}
		}
		return calSHA;
}

void storeInChunkBuffer(long startIndex, char *miniChunkBuffer, char *chunkBuffer){
    for(long i = 0; i < miniChunkSize; i++){
        chunkBuffer[startIndex++] = miniChunkBuffer[i];
    }
}

void retrieveDownloadInProgress(string userId){
    downloadInProgress.clear();
    ifstream fin;

    string completePath = "./UserDB/" + userId + "/D.txt";
    fin.open(completePath);

    while (fin) {
        string fileHash, input;
        getline(fin, fileHash);
        if(fileHash == "")   break;

        getline(fin, input);
        vector<string> parse = commandParser(input);
        struct fileMetaData *newFile = new fileMetaData;
        
        long ind = 0;
        newFile->fileName = parse[ind++];
        newFile->filePath = parse[ind++];
        newFile->fileSize = stol(parse[ind++]);
        newFile->groupId = parse[ind++];
        newFile->noOfChunks = stol(parse[ind++]);
        newFile->bitmap = vector<int>();
        newFile->seederInfo = unordered_map<string, pair<string, string>>();

        for(int i = 0; i < newFile->noOfChunks; i++){
            newFile->bitmap.push_back(stoi(parse[ind++]));
        }

        downloadInProgress[fileHash] = newFile;
    }
    fin.close();
}

void retrieveDownloadCompleted(string userId){
    downloadCompleted.clear();
    ifstream fin;

    string completePath = "./UserDB/" + userId + "/C.txt";
    fin.open(completePath);

    while (fin) {
        string fileHash, input;
        getline(fin, fileHash);
        if(fileHash == "")   break;

        getline(fin, input);
        vector<string> parse = commandParser(input);
        struct fileMetaData *newFile = new fileMetaData;
        
        long ind = 0;
        newFile->fileName = parse[ind++];
        newFile->filePath = parse[ind++];
        newFile->fileSize = stol(parse[ind++]);
        newFile->groupId = parse[ind++];
        newFile->noOfChunks = stol(parse[ind++]);
        newFile->bitmap = vector<int>();
        newFile->seederInfo = unordered_map<string, pair<string, string>>();

        for(int i = 0; i < newFile->noOfChunks; i++){
            newFile->bitmap.push_back(stoi(parse[ind++]));
        }

        downloadCompleted[fileHash] = newFile;
    }
    fin.close();
}

void updateDownloadInProgress(string userId){
    ofstream fout;
    string completePath = "./UserDB/" + userId + "/D.txt";
    fout.open(completePath);
    string line;
    for(auto file : downloadInProgress){
        fout << file.first << "\n";
        line = file.second->fileName + " " + file.second->filePath + " " + to_string(file.second->fileSize) + " " + file.second->groupId + " " + to_string(file.second->noOfChunks);
        for(auto chunkbit : file.second->bitmap){
            line = line + " " + to_string(chunkbit);
        }
        fout << line << "\n";
    }
    fout.close();
}

void updateDownloadCompleted(string userId){
    ofstream fout;
    string completePath = "./UserDB/" + userId + "/C.txt";
    fout.open(completePath);
    string line;
    for(auto file : downloadCompleted){
        fout << file.first << "\n";
        line = file.second->fileName + " " + file.second->filePath + " " + to_string(file.second->fileSize) + " " + file.second->groupId + " " + to_string(file.second->noOfChunks);
        for(auto chunkbit : file.second->bitmap){
            line = line + " " + to_string(chunkbit);
        }
        fout << line << "\n";
    }
    fout.close();
}




//thread functions
void *downloadChunkThread(void *input){
    long startingIndex = ((struct thread_arg_downloadChunkThread *)input)->startingIndex;
    string fileHash = ((struct thread_arg_downloadChunkThread *)input)->fileHash;
    vector<vector<string>> chunkWiseSeederList = ((struct thread_arg_downloadChunkThread *)input)->chunkWiseSeederList;
    struct fileMetaData *newFile = new fileMetaData;

    if(downloadRequest.find(fileHash) != downloadRequest.end()){
        newFile = downloadRequest[fileHash];
    }
    else if(downloadInProgress.find(fileHash) != downloadInProgress.end()){
        newFile = downloadInProgress[fileHash];
    }

    //Create file and open it
    string fileName = newFile->fileName;
    string completePath = newFile->filePath;
    int fw = open(completePath.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR); 

    //Actual file download starts
    for(long i = startingIndex; i < newFile->noOfChunks; i += 4){

        long chunkNo = stol(chunkWiseSeederList[i][0]);
        long totalMiniChunks = (chunkNo != newFile->noOfChunks - 1) ? 32 : 1 + (newFile->fileSize - chunkNo * chunkSize - 1) / miniChunkSize;
        bool isChunkDownloaded = false;
        
        for(long j = 1; j < chunkWiseSeederList[i].size(); j++){
            string peerUserName = chunkWiseSeederList[i][j];
            string peerIp = newFile->seederInfo[peerUserName].first;
            int peerPort = stoi(newFile->seederInfo[peerUserName].second);

            int sock = 0, client_fd;
            struct sockaddr_in serv_addr;
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                close(sock);
                continue;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(peerPort);
            if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
                close(sock);
                continue;
            }

            if ((client_fd = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
                close(sock);
                continue;
            }

            //request for chunknumber i with sha
            string message = "getChunk " + to_string(chunkNo) + " " + peerUserName;
            send(sock, message.c_str(), message.size(), 0);

            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            string response = convertToString(buffer);
            if(response == "Logged out"){
                close(sock);
                continue;
            }

            //send file sha
            send(sock, fileHash.c_str(), fileHash.size(), 0);

            //receive chunkSHA
            bzero(buffer, sizeof(buffer));
            recv(sock, buffer, 1024, 0);
            string recvChunkSHA = convertToString(buffer);

            //receive minichunks
            message = "ready";
            char miniChunkBuffer[16384] = {0}, chunkBuffer[524288] = {0};
            long valread = 0;
            for(long k = 0; k < totalMiniChunks; k++){
                send(sock, message.c_str(), message.size(), 0);

                bzero(miniChunkBuffer, sizeof(miniChunkBuffer));
                valread += recv(sock, miniChunkBuffer, sizeof(miniChunkBuffer), 0);
                storeInChunkBuffer(k * miniChunkSize, miniChunkBuffer, chunkBuffer);
            }

            //calulating chunkSHA and validating
            string calChunkSHA = chunkSHA(chunkBuffer, valread);
            if(recvChunkSHA != calChunkSHA){
                close(sock);
                continue;
            }

            // cout << chunkNo << " " << valread << "\n";

            isChunkDownloaded = true;
            pwrite(fw, chunkBuffer, valread, chunkNo * chunkSize);
            close(sock);    break;
        }
        if(isChunkDownloaded == false){
            cout << "File not downloaded. Not enough seeders available!" << "\n";
            close(fw);
            pthread_exit(NULL);
        }

        if(i == 0){
            //First chunk downloaded / Updating tracker

            int sock = 0, client_fd;
            struct sockaddr_in serv_addr;
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                printf("\n Socket creation error \n");
                pthread_exit(NULL);
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(trackerPort);
            if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
                printf("\nInvalid address/ Address not supported \n");
                pthread_exit(NULL);
            }

            if ((client_fd = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
                printf("\nConnection Failed \n");
                pthread_exit(NULL);
            }

            string line = "upload_file " + newFile->fileName + " " + to_string(newFile->fileSize) + " " + newFile->groupId + " " + to_string(newFile->noOfChunks) + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0); 

            send(sock, fileHash.c_str(), fileHash.size(), 0);

            bzero(buffer, sizeof(buffer));
            recv(sock, buffer, 1024, 0);
            string response = convertToString(buffer);

            close(sock);

            downloadInProgress[fileHash] = downloadRequest[fileHash];
            downloadRequest.erase(fileHash);
        }
        while(downloadInProgress.find(fileHash) == downloadInProgress.end()){

        }
        downloadInProgress[fileHash]->bitmap[chunkNo] = 1;
    }

    close(fw);
    pthread_exit(NULL);
}

void *downloadFileThread(void *input){

    // cout << "Download in progress..." << "\n";

    string fileHash = ((struct arg_struct*)input)->data;
    struct fileMetaData *newFile = downloadRequest[fileHash];
    vector<vector<string>> chunkWiseSeederList(newFile->noOfChunks); 
    for(long i = 0; i < newFile->noOfChunks; i++){
        chunkWiseSeederList[i].push_back(to_string(i));
    }         

    //traverse through every seeder and get the list of chunks available for every user
    for(auto seeder : newFile->seederInfo){
        string userName = seeder.first;
        string newIp = seeder.second.first;
        int newPort = stoi(seeder.second.second);

        int sock = 0, client_fd;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            pthread_exit(NULL);
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(newPort);
        if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            pthread_exit(NULL);
        }

        if ((client_fd = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
            printf("\nConnection Failed \n");
            pthread_exit(NULL);
        }
        
        string message = fileHash;
        send(sock, message.c_str(), message.size(), 0);

        char buffer[1024] = {0};
        recv(sock, buffer, 1024, 0);
        long count = stol(convertToString(buffer));

        message = "ready";
        for(long i = 0; i < count; i++){
            send(sock, message.c_str(), message.size(), 0);

            bzero(buffer, sizeof(buffer));
            recv(sock, buffer, 1024, 0);
            long chunkNo = stol(convertToString(buffer));
            chunkWiseSeederList[chunkNo].push_back(userName);
        }
        close(sock);
    }

    //Rarest piece first piece selection algorithm
    sort(chunkWiseSeederList.begin(), chunkWiseSeederList.end(), comp());

    pthread_t threadPool1, threadPool2, threadPool3, threadPool4;

    struct thread_arg_downloadChunkThread *newArg1 = new thread_arg_downloadChunkThread;
    newArg1->fileHash = fileHash;
    newArg1->chunkWiseSeederList = chunkWiseSeederList;
    newArg1->startingIndex = 0;
    pthread_create(&threadPool1, NULL, downloadChunkThread, (void *)newArg1);
    pthread_join(threadPool1, NULL);

    struct thread_arg_downloadChunkThread *newArg2 = new thread_arg_downloadChunkThread;
    newArg2->fileHash = fileHash;
    newArg2->chunkWiseSeederList = chunkWiseSeederList;
    newArg2->startingIndex = 1;
    pthread_create(&threadPool2, NULL, downloadChunkThread, (void *)newArg2);
    pthread_join(threadPool2, NULL);

    struct thread_arg_downloadChunkThread *newArg3 = new thread_arg_downloadChunkThread;
    newArg3->fileHash = fileHash;
    newArg3->chunkWiseSeederList = chunkWiseSeederList;
    newArg3->startingIndex = 2;
    pthread_create(&threadPool3, NULL, downloadChunkThread, (void *)newArg3);
    pthread_join(threadPool3, NULL);

    struct thread_arg_downloadChunkThread *newArg4 = new thread_arg_downloadChunkThread;
    newArg4->fileHash = fileHash;
    newArg4->chunkWiseSeederList = chunkWiseSeederList;
    newArg4->startingIndex = 3;
    pthread_create(&threadPool4, NULL, downloadChunkThread, (void *)newArg4);
    pthread_join(threadPool4, NULL);
    
    
    downloadCompleted[fileHash] = downloadInProgress[fileHash];
    downloadInProgress.erase(fileHash);
    
    // cout << "Download completed" << "\n";
    
    pthread_exit(NULL);
}

void *servingPeerRequest(void *arg){
    int new_socket = (long)arg;

    char buffer[1024] = {0};
    recv(new_socket, buffer, 1024, 0);
    string message = convertToString(buffer);

    if(message.substr(0, 8) == "getChunk"){
        vector<string> parse = commandParser(message);
        long chunkNo = stol(parse[1]);
        string peerUserName = parse[2];

        if(currUser->isLoggedIn == false || currUser->userId != peerUserName){
            string response = "Logged out";
            send(new_socket, response.c_str(), response.size(), 0);
            close(new_socket);
            pthread_exit(NULL);
        }

        message = "ready";
        send(new_socket, message.c_str(), message.size(), 0);

        bzero(buffer, sizeof(buffer));
        recv(new_socket, buffer, 1024, 0);
        string fileHash = convertToString(buffer);

        struct fileMetaData *newFile;
        if(downloadInProgress.find(fileHash) != downloadInProgress.end()){
            newFile = downloadInProgress[fileHash];
        }
        else if(downloadCompleted.find(fileHash) != downloadCompleted.end()){
            newFile = downloadCompleted[fileHash];
        }

        string fileName = newFile->fileName;
        string completePath = newFile->filePath;
        long totalMiniChunks = (chunkNo != newFile->noOfChunks - 1) ? 32 : 1 + (newFile->fileSize - chunkNo * chunkSize - 1) / miniChunkSize;

        int fr = open(completePath.c_str(), O_RDONLY);

        //send chunkSHA
        char chunkBuffer[524288] = {0};
        long nread = pread(fr, chunkBuffer, sizeof(chunkBuffer), chunkNo * chunkSize);
        string calSHA = chunkSHA(chunkBuffer, nread);
        send(new_socket, calSHA.c_str(), calSHA.size(), 0);
        

        char miniChunkBuffer[16384] = {0};
        long tread = 0;
        for(int k = 0; k < totalMiniChunks; k++){
            bzero(buffer, sizeof(buffer));
            recv(new_socket, buffer, 1024, 0);

            bzero(miniChunkBuffer, sizeof(miniChunkBuffer));
            nread = pread(fr, miniChunkBuffer, sizeof(miniChunkBuffer), chunkNo * chunkSize + k * miniChunkSize);
            send(new_socket, miniChunkBuffer, nread, 0);
            tread += nread;
        }

        // cout << chunkNo << " " << tread << "\n";
        close(fr);
    }
    else{
        string fileHash = message;
        struct fileMetaData *newFile;
        if(downloadInProgress.find(fileHash) != downloadInProgress.end()){
            newFile = downloadInProgress[fileHash];
        }
        else if(downloadCompleted.find(fileHash) != downloadCompleted.end()){
            newFile = downloadCompleted[fileHash];
        }

        long count = 0;
        for(long i = 0; i < newFile->noOfChunks; i++){
            if(newFile->bitmap[i] == 1){
                count++;
            }
        }

        string message = to_string(count);
        send(new_socket, message.c_str(), message.size(), 0);

        for(long i = 0; i < newFile->noOfChunks; i++){
            if(newFile->bitmap[i] == 0){
                continue;
            }
            bzero(buffer, sizeof(buffer));
            recv(new_socket, buffer, 1024, 0);

            message = to_string(i);
            send(new_socket, message.c_str(), message.size(), 0);
        }
    }
    close(new_socket);
    pthread_exit(NULL);
}

void *listeningPeer(void *arg) {

    int server_fd;
    long new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        pthread_exit(NULL);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf("\n setsockopt error \n");
        pthread_exit(NULL);
    }
    address.sin_family = AF_INET;
    address.sin_port = htons(thisPort);
    if (inet_pton(AF_INET, thisIp.c_str(), &address.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        pthread_exit(NULL);
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("\n bind error \n");
        pthread_exit(NULL);
    }

    if (listen(server_fd, 3) < 0) {
        printf("\n listen error \n");
        pthread_exit(NULL);
    }

    cout << "\n" << "----------------------------Peer----------------------------" << "\n";

    while(true){

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            printf("\n ClieconnectPeernt connection request accept failed \n");
            break;
        }
        else{
            pthread_t ptid;
            pthread_create(&ptid, NULL, servingPeerRequest, (void *)new_socket);
            pthread_detach(ptid);
        }   
    }

    shutdown(server_fd, SHUT_RDWR); 
    pthread_exit(NULL);
}

vector<string> commandParser(string command){
    command += ' ';
    long n =  command.size();
    string temp = "";
    vector<string> parse;
    for(long i = 0; i < n; i++){
        if(command[i] == ' '){
            parse.push_back(temp);
            temp = "";
        }
        else{
            temp = temp + command[i];
        }
    }
    return parse;
}

void driverCode(){

    //Permanent socket to connect with tracker
    int sock = 0, client_fd;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(trackerPort);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if ((client_fd = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("\nConnection Failed \n");
        return;
    }

    while(true){

        string input;
        getline(cin, input);
        vector<string> parse = commandParser(input);
        int n = parse.size();
        if(n == 0)  continue;
        string command = parse[0];

        if(command == "create_user"){
            if(n != 3){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            if(currUser->isLoggedIn == true){
                cout << "Used already logged in" << "\n";
                continue;
            }
            string userId = parse[1];
            string password = parse[2];

            string line = command + " " + userId + " " + password;
            send(sock, line.c_str(), line.size(), 0);     //sending command, username, password

            string response;
            char buffer[1024] = {0};
            int valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";

            string path = "./UserDB/" + userId;
            mkdir(path.c_str(), 0777);
        }
        else if(command == "login"){
            if(n != 3){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            if(currUser->isLoggedIn == true){
                cout << "Used already logged in" << "\n";
                continue;
            }

            string userId = parse[1];
            string password = parse[2];

            string line = command + " " + userId + " " + password + " " + thisIp + " " + to_string(thisPort);
            send(sock, line.c_str(), line.size(), 0);     //sending command, username, password

            string response;
            char buffer[1024] = {0};
            int valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";

            if(response == "Login successfull"){
                currUser->isLoggedIn = true;
                currUser->userId = userId;

                retrieveDownloadInProgress(userId);
                retrieveDownloadCompleted(userId);
            }
        }
        else if(currUser->isLoggedIn == false){
            cout << "Please login first" << "\n";
        }
        else if(command == "create_group"){
            if(n != 2){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];

            string line = command + " " + groupId + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            int valread;
            valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";
        }
        else if(command == "join_group"){
            if(n != 2){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];

            string line = command + " " + groupId + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            int valread;
            valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";
        }
        else if(command == "leave_group"){
            if(n != 2){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];

            string line = command + " " + groupId + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            int valread;
            valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";
        }
        else if(command == "list_requests"){
            if(n != 2){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];

            string line = command + " " + groupId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);
            if(response == "Group doesn't exists"){
                cout << response << "\n";
                continue;
            }
            long count = stol(response);
            if(count == 0){
                cout << "No requests to display" << "\n";
            }
            string message = "ready";
            for(long i = 0; i < count; i++){
                send(sock, message.c_str(), message.size(), 0);

                bzero(buffer, sizeof(buffer));
                recv(sock, buffer, 1024, 0);
                response = convertToString(buffer);
                cout << response << "\n";
            }
        }
        else if(command == "accept_request"){
            if(n != 3){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];
            string userId = parse[2];

            string line = command + " " + groupId + " " + userId + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            int valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";
        }
        else if(command == "list_groups"){
            if(n != 1){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            send(sock, command.c_str(), command.size(), 0);

            string response;
            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);
            long count = stol(response);
            if(count == 0){
                cout << "No groups to display" << "\n";
            }

            string message = "ready";
            for(long i = 0; i < count; i++){
                send(sock, message.c_str(), message.size(), 0);
                bzero(buffer, sizeof(buffer));
                recv(sock, buffer, 1024, 0);
                response = convertToString(buffer);
                cout << response << "\n";
            }
        }
        else if(command == "list_files"){
            if(n != 2){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];

            string line = command + " " + groupId + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);
            if(response == "Group doesn't exists" || response == "User is not part of the group"){
                cout << response << "\n";
                continue;
            }
            
            long count = stol(response);
            if(count == 0){
                cout << "No files available for download" << "\n";
            }
            string message = "ready";
            for(long i = 0; i < count; i++){
                send(sock, message.c_str(), message.size(), 0);
                bzero(buffer, sizeof(buffer));
                recv(sock, buffer, 1024, 0);
                response = convertToString(buffer);
                parse = commandParser(response);
                string fileName = parse[0];
                cout << fileName << "\t\t";
                calFileSize(stol(parse[1]));
            }         
        }
        else if(command == "upload_file"){
            if(n != 3){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string filePath = parse[1];
            string groupId = parse[2];
            string response;

            char buffer[1024] = {0};
            string fileHash = calFileHash(filePath);
            if(downloadCompleted.find(fileHash) != downloadCompleted.end()){
                cout << "File already uploaded" << "\n";
                continue;
            }

            struct fileMetaData *newfile = new fileMetaData;
            struct stat st;
            stat(filePath.c_str(), &st);

            newfile->fileName = calFileName(filePath);
            newfile->filePath = filePath;
            newfile->fileSize = st.st_size;
            newfile->groupId = groupId;
            newfile->noOfChunks = 1 + (newfile->fileSize - 1) / chunkSize;
            vector<int> temp(newfile->noOfChunks, 1);
            newfile->bitmap = temp;

            //send other details
            string line = command  + " " + newfile->fileName + " " + to_string(newfile->fileSize) + " " + newfile->groupId + " " + to_string(newfile->noOfChunks) + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            bzero(buffer, sizeof(buffer));
            recv(sock, buffer, 1024, 0); 

            //send filehash
            send(sock, fileHash.c_str(), fileHash.size(), 0);

            bzero(buffer, sizeof(buffer));
            int valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";
            
            if(response == "File uploaded successfully"){
                downloadCompleted[fileHash] = newfile;
            }
        }
        else if(command == "download_file"){         //special case, create a new thread
            if(n != 4){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];
            string fileName = parse[2];
            string destPath = parse[3];

            string line = command + " " + groupId + " " + fileName + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);

            if(response == "Group doesn't exists" || response == "User is not part of the group" || response == "File is not shared in group"){
                cout << response << "\n";
                continue;
            }

            //Downloading process starts
            string fileHash = response;

            line = "ready";
            send(sock, line.c_str(), line.size(), 0);

            bzero(buffer, sizeof(buffer));
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);
            vector<string> metadata = commandParser(response);
            string fileSize = metadata[1];
            string noOfChunks = metadata[2];
            long count = stol(metadata[3]); 
            if(count == 0){
                cout << "No seeders available for download" << "\n";
                continue;
            }

            unordered_map<string, pair<string, string>> seederInfo;
            string message = "ready";
            for(long i = 0; i < count; i++){
                send(sock, message.c_str(), message.size(), 0);

                bzero(buffer, sizeof(buffer));
                recv(sock, buffer, 1024, 0);
                response = convertToString(buffer);
                vector<string> parsedSeederInfo = commandParser(response);
                seederInfo[parsedSeederInfo[0]] = {parsedSeederInfo[1], parsedSeederInfo[2]};       //{userName, {ip, port}}
            }

            // cout << "Seeder list" << "\n";
            // for(auto avail : seederInfo){
            //     cout << avail.first << "\n";
            // }
            //storing file details in a structure
            struct fileMetaData *newFile = new fileMetaData;
            newFile->fileName = fileName;
            newFile->filePath = destPath + "/" + fileName;
            newFile->fileSize = stoll(fileSize);
            newFile->groupId = groupId;
            newFile->noOfChunks = 1 + (newFile->fileSize - 1) / 524288;
            newFile->bitmap = vector<int>(newFile->noOfChunks, 0);
            newFile->seederInfo = seederInfo;

            downloadRequest[fileHash] = newFile;                            //putting file in a download queue

            struct arg_struct *input = new arg_struct;
            input->port = 0;
            input->data = fileHash;
            
            pthread_t ptidDownloadFile;
            pthread_create(&ptidDownloadFile, NULL, downloadFileThread, (void *)input);            
        }
        else if(command == "logout"){
            if(n != 1){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string line = command + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            int valread = recv(sock, buffer, 1024, 0);
            if (valread <= 0) {
                cout << "Failure in executing command" << endl;
                continue;
            }
            response = convertToString(buffer);
            cout << response << "\n";

            updateDownloadInProgress(currUser->userId);
            updateDownloadCompleted(currUser->userId);
            downloadRequest.clear();
            downloadInProgress.clear();
            downloadCompleted.clear();

            currUser->isLoggedIn = false;
            currUser->userId = "";
        }
        else if(command == "show_downloads"){
            if(n != 1){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            int count = 0;
            for(auto file : downloadRequest){
                int totalChunks = file.second->noOfChunks;
                int chunksDownloaded = 0;
                for(auto chunkbit : file.second->bitmap){
                    if(chunkbit == 1)   chunksDownloaded++;
                }
                int percentageDownload = (chunksDownloaded * 100) / totalChunks;
                cout << "[D] " << file.second->groupId << "\t" << file.second->fileName << "\t" << percentageDownload << "%\n";
                count++;
            }
            for(auto file : downloadInProgress){
                int totalChunks = file.second->noOfChunks;
                int chunksDownloaded = 0;
                for(auto chunkbit : file.second->bitmap){
                    if(chunkbit == 1)   chunksDownloaded++;
                }
                int percentageDownload = (chunksDownloaded * 100) / totalChunks;
                cout << "[D] " << file.second->groupId << "\t" << file.second->fileName << "\t" << percentageDownload << "%\n";
                count++;
            }
            for(auto file : downloadCompleted){
                int totalChunks = file.second->noOfChunks;
                int chunksDownloaded = 0;
                for(auto chunkbit : file.second->bitmap){
                    if(chunkbit == 1)   chunksDownloaded++;
                }
                int percentageDownload = (chunksDownloaded * 100) / totalChunks;
                cout << "[C] " << file.second->groupId << "\t" << file.second->fileName << "\t" << percentageDownload << "%\n";
                count++;
            }
            if(count == 0){
                cout << "No files to display" << "\n";
            }
        }
        else if(command == "stop_share"){
            if(n != 3){
                cout << "Invalid number of arguments" << "\n";
                continue;
            }
            string groupId = parse[1];
            string fileName = parse[2];

            string line = command + " " + groupId + " " + fileName + " " + currUser->userId;
            send(sock, line.c_str(), line.size(), 0);

            string response;
            char buffer[1024] = {0};
            recv(sock, buffer, 1024, 0);
            response = convertToString(buffer);

            cout << response << "\n";
            if(response != "Group doesn't exists" && response != "User is not part of the group" && response != "File not uploaded by user"){
                string message = "ready";
                send(sock, message.c_str(), message.size(), 0);

                bzero(buffer, sizeof(buffer));
                recv(sock, buffer, 1024, 0);
                string fileSHA = convertToString(buffer);

                if(downloadRequest.find(fileSHA) != downloadRequest.end()){
                    downloadRequest.erase(fileSHA);
                }
                else if(downloadInProgress.find(fileSHA) != downloadInProgress.end()){
                    downloadInProgress.erase(fileSHA);
                }
                else if(downloadCompleted.find(fileSHA) != downloadCompleted.end()){
                    downloadCompleted.erase(fileSHA);
                }
            }
        }
        else{
            cout << "Invalid command" << "\n";
        }
    }
    close(sock);
}

int main(int argc, char** argv)
{
    if(argc != 3){
        cout << "Invalid number of arguments" << "\n";
        return 0;
    }
    
    string clientDetails = argv[1];
    thisIp = clientDetails.substr(0, clientDetails.find(":"));
    thisPort = stol(clientDetails.substr(clientDetails.find(":") + 1, clientDetails.size() - thisIp.size() - 1));
    string trackerInfo = argv[2];

    ifstream fin;
    fin.open(trackerInfo);

    string input;
    int count  = 0;
    while (fin) {
        count++;
        getline(fin, input);
        if(input == "")   break;

        if(count == 1){
            vector<string> parse = commandParser(input);
            trackerIp = parse[0];
            trackerPort = stol(parse[1]);
            break;
        }
    }
    fin.close();

    pthread_t ptidListen;
    pthread_create(&ptidListen, NULL, listeningPeer, NULL);

    driverCode();

    return 0;
}