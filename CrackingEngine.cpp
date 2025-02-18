/*
 * CrackingEngine.cpp
 *
 *  Created on: Sep 4, 2012
 *      Author: moloch
 */

#include "CrackingEngine.h"

CrackingEngine::CrackingEngine(std::string hashType): solved(0), threadCount(1), debug(false) {
	pyThreadState = PyEval_SaveThread();
	wordListMutex = new boost::mutex();
	results = new Results();
	resultsMutex = new boost::mutex();
	stdoutMutex = new boost::mutex();
	hashesMutex = new boost::mutex();
	this->hashType = hashType;
}

CrackingEngine::~CrackingEngine() {
	delete wordListMutex;
	delete results;
	delete resultsMutex;
	delete stdoutMutex;
	delete hashesMutex;
	PyEval_RestoreThread(pyThreadState);
	pyThreadState = NULL;
}

/*
 * Called to start the actual cracking, spawns worker threads
 */
Results CrackingEngine::crack() {
	if (debug) {
		std::cout << INFO << "Started cracking, please wait ..." << std::endl;
	}
	if (!hashes.empty()) {
		boost::thread_group threadGroup;
		for (unsigned int threadId = 0; threadId < threadCount; ++threadId) {
			threadGroup.create_thread(boost::bind(&CrackingEngine::workerThread, this, threadId));
		}
		if (debug) {
			stdoutMutex->lock();
			std::cout << INFO << "Waiting for worker threads to finish ..." << std::endl;
			stdoutMutex->unlock();
		}
		threadGroup.join_all();
		if (debug) {
			std::cout << INFO << "All worker threads completed." << std::endl;
		}
	}
	return (*results);
}

/*
 * Sets the number of worker threads to spawn; defaults to 1
 */
void CrackingEngine::setThreads(unsigned int threadCount) {
	this->threadCount = threadCount;
	if (debug) {
		std::cout << INFO << "Set number of threads: " << threadCount << std::endl;
	}
}

void CrackingEngine::setDebug(bool debug) {
	this->debug = debug;
	if (debug) {
		std::cout << INFO << "Set debug mode: ON" << std::endl;
	}
}

void CrackingEngine::setWords(std::queue<std::string>* words) {
	wordListQueue = words;
	if (debug) {
		std::cout << INFO << "Got " << wordListQueue->size() << " words." << std::endl;
	}
}

void CrackingEngine::setHashes(std::vector<std::string>& hashes) {
	this->hashes = hashes;
	if (debug) {
		std::cout << INFO << "Got " << hashes.size() << " hash(es) for cracking." << std::endl;
	}
}

/*
 * Executed as separate thread; computes hash of a word in the wordList
 * and compares the result to the hashes we're looking for, saves results.
 */
void CrackingEngine::workerThread(int threadId) {
	hashesMutex->lock();
	std::vector <std::string> hashList = hashes;
	hashesMutex->unlock();
	HashAlgorithm* algorithm = HashFactory::getInstance(hashType);
	std::string word;
	int count = 0;
	while(true) {
		wordListMutex->lock();
		resultsMutex->lock();
		if (wordListQueue->empty() || results->size() == hashList.size()) {
			resultsMutex->unlock();
			wordListMutex->unlock();
			break;
		} else {
			resultsMutex->unlock();
			word = wordListQueue->front();
			wordListQueue->pop();
			wordListMutex->unlock();
		}
		std::string digest = algorithm->hexdigest(word);
		count++;
		if (std::find(hashList.begin(), hashList.end(), digest) != hashList.end()) {
			if (debug) {
				stdoutMutex->lock();
				std::cout << INFO << "Thread #" << threadId <<" : " << digest << " -> " << word << std::endl;
				stdoutMutex->unlock();
			}
			resultsMutex->lock();
			(*results)[digest] = word;
			resultsMutex->unlock();
		}
	}
	delete algorithm;
	if (debug) {
		stdoutMutex->lock();
		std::cout << INFO << "Thread #" << threadId << " : Calculated " << count << " hash(es)." << std::endl;
		stdoutMutex->unlock();
	}
}



