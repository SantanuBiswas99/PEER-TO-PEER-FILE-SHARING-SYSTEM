#include <bits/stdc++.h>
#include <netinet/in.h>
#include<pthread.h>
#include<arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
using namespace std;

vector<string> dataParser(string);

struct arg_struct {
    int port;
    string data;
};

struct fileMetaData {
    string fileName;
    unsigned long long fileSize;
    string groupId;
    long long noOfChunks;
    unordered_set<string> seeders;
};

//global variables
int thisPort;
string thisIp;


//data structures
unordered_map<string, string> userDetails;                                     //{username, password}
unordered_map<string, pair<string, unordered_set<string>>> groupInfo;          //{groupId, <groupOwneruId, <uId>>}
unordered_map<string, pair<string, unordered_set<string>>> pendingRequest;     //{groupId, <groupOwneruId, <pending uid>>}
unordered_map<string, unordered_map<string, struct fileMetaData *>> fileInfo;                  //{groupId, {SHA, metadata}}
unordered_map<string, unordered_map<string, string>> filenameSHA;                                 //{groupId, {filename, SHA}} --> reverse mapping
unordered_map<string, pair<string, string>> loggedUsers;                //{username, {ip, port number}}

//utility function
string convertToString(char* a)
{
    int i,counter=0;
    while(a[counter]!='\0'){
        counter++;
    }
    string s = "";
    for (i = 0; i < counter; i++) {
        s = s + a[i];
    }
    return s;
}

bool doesGroupExists(string groupId){
    for(auto group : groupInfo){
        if(group.first == groupId){
            return true;
        }
    }
    return false;
}

bool isUserPartOfGroup(string userId, string groupId){
    if(groupInfo[groupId].first == userId)  return true;

    for(auto user : groupInfo[groupId].second){
        if(user == userId){
            return true;
        }
    }
    return false;
}

void updategroupInfotxt(){
    ofstream fout;
    fout.open("./TrackerDB/groupInfo.txt");
    string line;
    for(auto group : groupInfo){
        line = group.first + " " + group.second.first;
        for(auto user : group.second.second){
            line = line + " " + user;
        }
        fout << line << "\n";
    }
    fout.close();
}

void updatePendingRequesttxt(){
    ofstream fout;
    fout.open("./TrackerDB/pendingRequest.txt");
    
    for(auto currGroup : pendingRequest){
        string line = currGroup.first + " " + currGroup.second.first;
        for(auto pendingUsers : currGroup.second.second){
            line = line + " " + pendingUsers;
        }
        fout << line << "\n";
    }
    fout.close();
}

void updateFileInfo(){
    ofstream fout;
    fout.open("./TrackerDB/fileInfo.txt");
    
    for(auto currGroup : fileInfo){
        string line = currGroup.first + " " + to_string(currGroup.second.size());
        fout << line << "\n";
        for(auto file : currGroup.second){
            line = file.first;
            fout << line << "\n";
            line = file.second->fileName + " " + to_string(file.second->fileSize) + " " + file.second->groupId + " " + to_string(file.second->noOfChunks) + " " + to_string(file.second->seeders.size());
            for(auto seeder : file.second->seeders){
                line = line + " " + seeder;
            }
            fout << line << "\n";
        }
    }
    fout.close();
}

void updateFilenameSHA(){
    ofstream fout;
    fout.open("./TrackerDB/filenameSHA.txt");
    
    for(auto currGroup : filenameSHA){
        string line = currGroup.first + " " + to_string(currGroup.second.size());
        fout << line << "\n";
        for(auto file : currGroup.second){
            fout << file.first << "\n";
            fout << file.second << "\n";
        }
    }
    fout.close();
}

void *connectPeer(void *input){
    int newPort = ((struct arg_struct*)input)->port;
    string data = ((struct arg_struct*)input)->data;

    // cout << "Trying to connect" << "\n";
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
    send(sock, data.c_str(), data.size(), 0);
    close(sock);
    pthread_exit(NULL);
}

void *servingPeerRequest(void *arg){
    int new_socket = (long)arg;

    while(true){
        string input;
        char buffer[1024] = {0};
        int valread = recv(new_socket, buffer, 1024, 0);
        if (valread <= 0) {
            close(new_socket);
            pthread_exit(NULL);
        }
        input = convertToString(buffer);
        vector<string> parse = dataParser(input);
        string command = parse[0];

        if(command == "create_user"){
            string userId, password;
            userId = parse[1];
            password = parse[2];
            string message;

            bool flag = false;
            for(auto user : userDetails){
                if(user.first == userId){
                    message = "User already exists";
                    send(new_socket, message.c_str(), message.size(), 0);
                    flag = true;
                    break;
                }
            }
            if(flag == false){
                userDetails[userId] = password;

                ofstream fout;
                fout.open("./TrackerDB/userDetails.txt");
                string line;
                for(auto user : userDetails){
                    line = user.first + " " + user.second;
                    fout << line << endl;
                }
                message = "User created successfully";
                send(new_socket, message.c_str(), message.size(), 0);
            }
        }
        else if(command == "login"){        
            string userId, password, peerIp, peerPort;
            userId = parse[1];
            password = parse[2];
            peerIp = parse[3];
            peerPort = parse[4];
            string message;

            if(loggedUsers.find(userId) != loggedUsers.end()){
                message = "User already logged in from a different machine";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }

            bool flag = false;
            for(auto user : userDetails){
                if(user.first == userId && user.second == password){
                    message = "Login successfull";
                    send(new_socket, message.c_str(), message.size(), 0);
                    flag = true;
                    loggedUsers[userId] = {peerIp, peerPort};
                    break;
                }
            }
            if(flag == false){
                message = "Invalid username or password";
                send(new_socket, message.c_str(), message.size(), 0);
            }
        }
        else if(command == "create_group"){
            string groupId, userId;
            groupId = parse[1];
            userId = parse[2];
            string message;

            if(doesGroupExists(groupId) == true){
                message = "Group already exists";
                send(new_socket, message.c_str(), message.size(), 0);
            }
            else{
                groupInfo[groupId] = {userId, {}};
                pendingRequest[groupId] = {userId, {}};
                fileInfo[groupId] = {};
                filenameSHA[groupId] = {};

                updategroupInfotxt();
                updatePendingRequesttxt();
                updateFileInfo();
                updateFilenameSHA();
                message = "Group created successfully";
                send(new_socket, message.c_str(), message.size(), 0);
            }
        }
        else if(command == "join_group"){
            string groupId, userId;
            groupId = parse[1];
            userId = parse[2];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
            }
            else if(isUserPartOfGroup(userId, groupId) == true){
                message = "User is already part of the group";
            }
            else{
                pendingRequest[groupId].second.insert(userId);
                updatePendingRequesttxt();
                message = "Request sent successfully";
            }
            send(new_socket, message.c_str(), message.size(), 0);
        }
        else if(command == "leave_group"){
            string groupId, userId;
            groupId = parse[1];
            userId = parse[2];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(isUserPartOfGroup(userId, groupId) == false){
                message = "User is not part of the group";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(userId == groupInfo[groupId].first){                                //group owner is leaving
                if(groupInfo[groupId].second.size() == 0){
                    
                    groupInfo.erase(groupId);
                    pendingRequest.erase(groupId);
                    fileInfo.erase(groupId);
                    filenameSHA.erase(groupId);

                    updategroupInfotxt();
                    updatePendingRequesttxt();
                    updateFileInfo();
                    updateFilenameSHA();
                    message = "Group left successfully --> Group deleted";
                }
                else{
                    string newGroupOwnerId = *(groupInfo[groupId].second.begin());
                    groupInfo[groupId].second.erase(newGroupOwnerId);
                    groupInfo[groupId].first = newGroupOwnerId;
                    pendingRequest[groupId].first = newGroupOwnerId;

                    updategroupInfotxt();
                    updatePendingRequesttxt();
                    message = "Group left successfully";
                }
            }
            else{                                                                       //group member is leaving
                groupInfo[groupId].second.erase(userId);
                updategroupInfotxt();
                message = "Group left successfully";
            }

            send(new_socket, message.c_str(), message.size(), 0);
            if(fileInfo.find(groupId) == fileInfo.end()){
                continue;
            }

            //update all files with user as a seeder
            unordered_map<string, struct fileMetaData *> newFileInfo;
            unordered_map<string, string> newFileNameSHA;

            for(auto file : fileInfo[groupId]){
                string fileSHA = file.first;
                struct fileMetaData *newFile = file.second;
                string fileName = newFile->fileName;
                
                if(newFile->seeders.find(userId) != newFile->seeders.end()){
                    
                    if(newFile->seeders.size() == 1){
                        
                    }
                    else{
                        newFile->seeders.erase(userId);
                        newFileInfo[fileSHA] = newFile;
                        newFileNameSHA[fileName] = fileSHA;
                    }
                }
                else{
                    newFileInfo[fileSHA] = newFile;
                    newFileNameSHA[fileName] = fileSHA;
                }
            }

            fileInfo[groupId] = newFileInfo;
            filenameSHA[groupId] = newFileNameSHA;
            updateFileInfo();
            updateFilenameSHA();
        }
        else if(command == "list_requests"){
            string groupId;
            groupId = parse[1];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
            }
            else{
                string count = to_string(pendingRequest[groupId].second.size());
                send(new_socket, count.c_str(), count.size(), 0);

                char buffer[1024] = {0};
                for(auto user : pendingRequest[groupId].second){
                    bzero(buffer, sizeof(buffer));
                    recv(new_socket, buffer, 1024, 0);
                    send(new_socket, user.c_str(), user.size(), 0);
                }
            }
        }
        else if(command == "accept_request"){
            string groupId, userId, currUserId;
            groupId = parse[1];
            userId = parse[2];
            currUserId = parse[3];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(groupInfo[groupId].first != currUserId){
                message = "Can only be accepted by group owner";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }

            bool flag = false;
            for(auto user : pendingRequest[groupId].second){
                if(user == userId){
                    flag = true;
                    break;
                }
            }

            if(flag == false){
                message = "Request not found";
            }
            else{
                groupInfo[groupId].second.insert(userId);
                pendingRequest[groupId].second.erase(userId);

                updategroupInfotxt();
                updatePendingRequesttxt();
                message = "User added to the group";
            }

            send(new_socket, message.c_str(), message.size(), 0);
        }
        else if(command == "list_groups"){
            string count = to_string(groupInfo.size());
            send(new_socket, count.c_str(), count.size(), 0);
            char buffer[1024] = {0};
            for(auto group : groupInfo){
                bzero(buffer, sizeof(buffer));
                recv(new_socket, buffer, 1024, 0);
                send(new_socket, group.first.c_str(), group.first.size(), 0);
            }
        }
        else if(command == "list_files"){
            string groupId, userId;
            groupId = parse[1];
            userId = parse[2];
            string message;
            
            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
            }
            else if(isUserPartOfGroup(userId, groupId) == false){
                message = "User is not part of the group";
                send(new_socket, message.c_str(), message.size(), 0);
            }
            else{
                string count = to_string(fileInfo[groupId].size());
                send(new_socket, count.c_str(), count.size(), 0);

                char buffer[1024] = {0};
                string fileDetails;
                for(auto file : fileInfo[groupId]){
                    bzero(buffer, sizeof(buffer));
                    recv(new_socket, buffer, 1024, 0);

                    fileDetails = file.second->fileName + " " + to_string(file.second->fileSize);
                    send(new_socket, fileDetails.c_str(), fileDetails.size(), 0);
                }
            }   
        }
        else if(command == "upload_file"){
            string fileHash, fileName, fileSize, groupId, noOfChunks, userId;
            fileName = parse[1];    fileSize = parse[2];
            groupId = parse[3];     noOfChunks = parse[4];  userId = parse[5];

            string message = "ready";
            send(new_socket, message.c_str(), message.size(), 0);

            bzero(buffer, sizeof(buffer));
            recv(new_socket, buffer, 1024, 0); 
            fileHash = convertToString(buffer);

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
            }
            else if(isUserPartOfGroup(userId, groupId) == false){
                message = "User is not part of the group";
            }
            else if(fileInfo[groupId].find(fileHash) != fileInfo[groupId].end()){          //file already exists, need to update the seeder list
                fileInfo[groupId][fileHash]->seeders.insert(userId);
                message = "File uploaded successfully";
            }
            else{                                                   //file not present, need to update fileinfo, filenameSHA
                struct fileMetaData *newFile = new fileMetaData;
                newFile->fileName = fileName;
                newFile->fileSize = stoll(fileSize);
                newFile->groupId = groupId;
                newFile->noOfChunks = stoll(noOfChunks);
                newFile->seeders.insert(userId);

                fileInfo[groupId][fileHash] = newFile;
                filenameSHA[groupId][fileName] = fileHash;
                message = "File uploaded successfully";
            }
            updateFileInfo();
            updateFilenameSHA();
            send(new_socket, message.c_str(), message.size(), 0);
        }
        else if(command == "download_file"){         
            string groupId, fileName, userId;
            groupId = parse[1];
            fileName = parse[2];
            userId = parse[3];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(isUserPartOfGroup(userId, groupId) == false){
                message = "User is not part of the group";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(filenameSHA[groupId].find(fileName) == filenameSHA[groupId].end()){                 //File is not shared in the current group
                message = "File is not shared in group";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else{
                //Tracker shares metadata to peer
                string fileHash = filenameSHA[groupId][fileName];
                string fileName = fileInfo[groupId][fileHash]->fileName;
                string fileSize = to_string(fileInfo[groupId][fileHash]->fileSize);
                string noOfChunks = to_string(fileInfo[groupId][fileHash]->noOfChunks);
                unordered_set<string> seeders = fileInfo[groupId][fileHash]->seeders;
                
                int count = 0;
                for(auto seederUserName : seeders){
                    if(loggedUsers.find(seederUserName) != loggedUsers.end()){
                        count++;
                    }
                }
                //send filehash
                send(new_socket, fileHash.c_str(), fileHash.size(), 0);

                char buffer[1024] = {0};
                recv(new_socket, buffer, 1024, 0); 

                string size = to_string(count);
                string message = fileName + " " + fileSize + " " + noOfChunks + " " + size;
                send(new_socket, message.c_str(), message.size(), 0);
                
                for(auto seederUserName : seeders){
                    if(loggedUsers.find(seederUserName) == loggedUsers.end()){
                        continue;
                    }
                    bzero(buffer, sizeof(buffer));
                    recv(new_socket, buffer, 1024, 0);

                    //send userId, Ip ,port
                    message = seederUserName + " " + loggedUsers[seederUserName].first + " " + loggedUsers[seederUserName].second;
                    send(new_socket, message.c_str(), message.size(), 0);
                }
                //file metadata transfer completed
            }
        }
        else if(command == "logout"){
            string userId;
            userId = parse[1];
            string message;

            loggedUsers.erase(userId);
            message = "Logged out successfully";
            send(new_socket, message.c_str(), message.size(), 0);
        }
        else if(command == "show_downloads"){
                
        }
        else if(command == "stop_share"){
            string groupId, fileName, userId;
            groupId = parse[1];     fileName = parse[2];    userId = parse[3];
            string message;

            if(doesGroupExists(groupId) == false){
                message = "Group doesn't exists";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(isUserPartOfGroup(userId, groupId) == false){
                message = "User is not part of the group";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else if(filenameSHA[groupId].find(fileName) == filenameSHA[groupId].end()){
                message = "File not uploaded by user";
                send(new_socket, message.c_str(), message.size(), 0);
                continue;
            }
            else{
                string fileSHA = filenameSHA[groupId][fileName];
                struct fileMetaData *newFile = fileInfo[groupId][fileSHA];
                if(newFile->seeders.find(userId) == newFile->seeders.end()){
                    message = "File not uploaded by user";
                    send(new_socket, message.c_str(), message.size(), 0);
                    continue;
                }
                else{
                    fileInfo[groupId][fileSHA]->seeders.erase(userId);
                    if(fileInfo[groupId][fileSHA]->seeders.size() == 0){
                        fileInfo[groupId].erase(fileSHA);
                        filenameSHA[groupId].erase(fileName);
                    }

                    message = "File is not shared anymore";
                    send(new_socket, message.c_str(), message.size(), 0);

                    char buffer[1024] = {0};
                    recv(new_socket, buffer, 1024, 0);

                    send(new_socket, fileSHA.c_str(), fileSHA.size(), 0);
                    updateFileInfo();
                    updateFilenameSHA();
                }
            }
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
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(thisPort);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("\n bind error \n");
        pthread_exit(NULL);
    }

    if (listen(server_fd, 3) < 0) {
        printf("\n listen error \n");
        pthread_exit(NULL);
    }

    cout << "\n" << "----------------------------Tracker----------------------------" << "\n";

    while(true){

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            printf("\n client connection request accept failed \n");
            break;
        }
        else{
            pthread_t ptid;
            pthread_create(&ptid, NULL, servingPeerRequest, (void *)new_socket);    
        }   
    }

    shutdown(server_fd, SHUT_RDWR); 
    pthread_exit(NULL);
}

vector<string> dataParser(string command){
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

void retrieveLoginInfo(){
    userDetails.clear();
    ifstream fin;
    fin.open("./TrackerDB/userDetails.txt");

    string input;
    while (fin) {
        getline(fin, input);
        if(input == "")   break;
        vector<string> parse = dataParser(input);
        userDetails[parse[0]] = parse[1];
    }
    fin.close();
}

void retrieveGroupInfo(){
    groupInfo.clear();
    ifstream fin;
    fin.open("./TrackerDB/groupInfo.txt");

    string input;
    while (fin) {
        getline(fin, input);
        if(input == "")   break;

        vector<string> parse = dataParser(input);
        groupInfo[parse[0]] = {parse[1], {}};
        for(long i = 2; i < parse.size(); i++){
            groupInfo[parse[0]].second.insert(parse[i]);
        }
    }
    fin.close();
}

void retrievePendingRequest(){
    pendingRequest.clear();
    ifstream fin;
    fin.open("./TrackerDB/pendingRequest.txt");

    string input;
    while (fin) {
        getline(fin, input);
        if(input == "")   break;

        vector<string> parse = dataParser(input);
        pendingRequest[parse[0]] = {parse[1], {}};
        for(long i = 2; i < parse.size(); i++){
            pendingRequest[parse[0]].second.insert(parse[i]);
        }
    }
    fin.close();
}

void retrieveFileInfo(){
    fileInfo.clear();

    ifstream fin;
    fin.open("./TrackerDB/fileInfo.txt");

    string input;
    while (fin) {
        getline(fin, input);
        if(input == "")   break;

        vector<string> parse = dataParser(input);
        string groupId = parse[0];
        long fileCount = stol(parse[1]);
        fileInfo[groupId] = {};

        for(long i = 0; i < fileCount; i++){
            string fileHash;
            getline(fin, fileHash);

            getline(fin, input);
            parse = dataParser(input);
            long ind = 0;
            struct fileMetaData *newFile = new fileMetaData;
            newFile->fileName = parse[ind++];
            newFile->fileSize = stoll(parse[ind++]);
            newFile->groupId = parse[ind++];
            newFile->noOfChunks = stol(parse[ind++]);
            long seedersCount = stol(parse[ind++]);

            newFile->seeders = unordered_set<string>();
            for(int j = 0; j < seedersCount; j++){
                newFile->seeders.insert(parse[ind++]);
            }
            fileInfo[groupId][fileHash] = newFile;
        }
    }
    fin.close();
}

void retrieveFilenameSHA(){
    filenameSHA.clear();

    ifstream fin;
    fin.open("./TrackerDB/filenameSHA.txt");

    string input;
    while (fin) {
        getline(fin, input);
        if(input == "")   break;

        vector<string> parse = dataParser(input);
        string groupId = parse[0];
        long count = stol(parse[1]);
        filenameSHA[groupId] = {};

        for(long i = 0; i < count; i++){
            string fileName, fileHash;
            getline(fin, fileName);
            getline(fin, fileHash);
            filenameSHA[groupId][fileName] = fileHash;
        }
    }
    fin.close();
}

void sessionInfo(string input){
    retrieveLoginInfo();
    retrieveGroupInfo();
    retrievePendingRequest();

    
    if(input == "Y" || input == "y" || input == "yes"){
        retrieveFileInfo();
        retrieveFilenameSHA();
    }
}

int main(int argc, char** argv)
{
    if(argc != 3){
        cout << "Invalid number of arguments" << "\n";
        return 0;
    }

    string trackerInfo = argv[1];
    long trackerNo = stol(argv[2]);

    ifstream fin;
    fin.open(trackerInfo);

    string input;
    int count  = 0;
    while (fin) {
        count++;
        getline(fin, input);
        if(input == "")   break;

        if(count == trackerNo){
            vector<string> parse = dataParser(input);
            thisIp = parse[0];
            thisPort = stol(parse[1]);
            break;
        }
    }
    fin.close();


    //retrive last session info
    cout << "Retrieve last session info [Y/n] : ";
    getline(cin, input);
    sessionInfo(input);
    
    pthread_t ptidListen;
    pthread_create(&ptidListen, NULL, listeningPeer, NULL);

    while(getline(cin, input)){
        if(input == "quit"){
            cout << "Terminating tracker service --bye" << "\n";
            return 0;
        }
    }

    return 0;
}