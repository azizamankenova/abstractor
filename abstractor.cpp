/**
 * @author Aziza Mankenova
 * Student ID 2018400387
 * 
 * The Abstractor is a multithreaded research tool for scientific literature. It focuses on
 * delivering fast and accurate results for given queries. The program takes a query as an input,
 * scans its file collection, finds the most relevant abstracts by measuring the similarities between
 * abstracts and the given query, then retrieves convenient sentences from the selected abstracts.
 * The program evenly spreads the job among the threads that separately calculate jaccard similarity
 * for each abstract. Mutexes were utilized since race conditions are possible.
 *
 */

#include <bits/stdc++.h>
#include <pthread.h>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace std;

//vector of query words
vector<string> query;
//queue of abstract file names
queue<string> abstracts;
//vector of pairs of jaccards scores and abstract names
vector< pair< float, string> > jaccard_scores;
//vector of pairs of summaries and abstract names
vector< pair< string, string> > summaries;
//outstream output file
ofstream output_file;
//mutexes
pthread_mutex_t mutex_abstracts = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sum = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_jac = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_output = PTHREAD_MUTEX_INITIALIZER;
//function that calculates jaccard similarity scores and finds the summary of the query words
void *jaccard_similarity(void *param);

int main(int argc, char *argv[]){
	//open input file given as an argument
	ifstream input_file(argv[1]);
	//open the output file given as an argument
	output_file.open(argv[2]);
	string first_line;
	//read the first line of input file
	getline(input_file, first_line);
	stringstream ss(first_line);	
	int T; // number of threads
	int A; // number of abstracts to be scanned
	int N; // number of abstracts to be returned
	//retrive number of threads, number of abstracts to be scanned and to be returned from the first line
	ss >> T >> A >> N;	
	string quer;
	//read the second line of input file
	getline(input_file, quer);
	stringstream qu(quer);
	string w;
	//retrive the query words into vector of strings from the second line
	while (qu >> w)
    {
        query.push_back(w);
    }
    //sort the query words
    sort(query.begin(), query.end());
    //retrive names of abstract files into vector of strings from the following lines
	for(int i = 0 ; i  < A; i++){
		string abs;
		getline(input_file, abs);
		abstracts.push(abs);
	}
	input_file.close();

	//create array if threads
	pthread_t thread_ids[T];
	//represents the attributes for the threads
    pthread_attr_t attr;
    // Get the default attributes defined by pthread
	pthread_attr_init(&attr);
	//create interger arrays for ids of threads
	int nums[T];
	for(int i = 0; i < T; i++){
		nums[i] = i;
	}
	// Create T number of threads, assign integer ids, which are then changed into char ids, as the argument to each thread
	for(int i = 0; i < T; i++){
		pthread_create(&thread_ids[i], &attr, &jaccard_similarity, &nums[i]);
	}
	//wait for the threads to terminate
    for (int i = 0; i <T; i++)
    {
        pthread_join(thread_ids[i], NULL); 
    }

    //sort jaccard scores
	sort(jaccard_scores.rbegin(), jaccard_scores.rend());

	//write the output to the output file with the scores and summary
	for(int i = 0; i < N; i++){
		output_file << "###\nResult " << i + 1 << ":\nFile: " << jaccard_scores[i].second << "\nScore: " << setprecision(4) << fixed << jaccard_scores[i].first << endl;
		auto index = distance(summaries.begin(), find_if(summaries.begin(), summaries.end(), [&](const auto& pair) { return pair.first == jaccard_scores[i].second; }));
		output_file << "Summary: " << summaries[index].second << endl;

	}
	output_file << "###" << endl;
	output_file.close();

	return 0;


}

//calculates jaccard similarity scores and finds the summary of the query words
void *jaccard_similarity(void *param)
{	

	int id = *(int *)param;
	//assign char id to the thread
	char t_id = (char)(id + 65);

	while(true){ 
		//sleep for 10 ms
		usleep(10000);
		//lock abstracts mutex while retrieving next abstract to be calculated
		pthread_mutex_lock(&mutex_abstracts);
		if(abstracts.empty()){
			pthread_mutex_unlock(&mutex_abstracts);
			break;
		}		
		string next_abstract = abstracts.front();
		abstracts.pop();
		pthread_mutex_unlock(&mutex_abstracts);
		//lock output mutex. Writing to output file the abstract name that the thread is calculating
		pthread_mutex_lock(&mutex_output);
		output_file << "Thread " << t_id << " is calculating " << next_abstract << endl;
		pthread_mutex_unlock(&mutex_output);
		//open the abstract file to be read
		ifstream abstr("../abstracts/" + next_abstract);
		string word;
		vector<string> words;
		string whole_text = "";
		//read the abstract word by word and write them into vector of strings
		while (abstr >> word)
	    {	
	    	whole_text += word;
	    	whole_text += " ";
	    	words.push_back(word);
	    }
	    abstr.close();
	    vector<string> sentences;
	    // find and write all the sentences into vector of sentences
	    string snt = ' ' + whole_text.substr(0, whole_text.find(".") + 1);
	    sentences.push_back(snt);
	    if(whole_text.find(".") != string::npos)
	    	whole_text = whole_text.substr(whole_text.find(".") + 1, whole_text.size() - snt.size());
	    while(whole_text.find(".") != string::npos){
	    	string sentence = whole_text.substr(0, whole_text.find(".") + 1);
	    	sentences.push_back(sentence);
	    	if(whole_text.find(".") != string::npos)
	    		whole_text = whole_text.substr(whole_text.find(".") + 1, whole_text.size() - sentence.size());
	    }

	    vector<string> intersection;
	    sort(words.begin(), words.end());
	 
	    //find the intersection of words and query vectors
	    set_intersection(query.begin(), query.end(), words.begin(), words.end(), back_inserter(intersection));

	    vector<string> unionn;
	    //find the union of words and query vectors
	    set_union(query.begin(), query.end(), words.begin(), words.end(), back_inserter(unionn));
	    //delete duplicates from the union vector
		unionn.erase( unique( unionn.begin(), unionn.end() ), unionn.end() );
		//calculate jaccard score
	    float jaccard = (float)intersection.size() / unionn.size();
	    //locks jaccard mutex and pushes pair of abstract name and its jaccard score into vector of jaccard scores
	    pthread_mutex_lock(&mutex_jac);
	    jaccard_scores.push_back(make_pair(jaccard, next_abstract));
		pthread_mutex_unlock(&mutex_jac);

		//finds summary by searching for the query words in each sentence
	    string summary = "";
	    for(string s : sentences){
	    	for(string q : query){
	    		if(s.find(" " + q + " ") != string::npos ){
	    			if(s.at(0) == ' '){
	    				s = s.substr(1, s.size() - 1);
	    			}
	    			summary += s + " ";
	    			break;
	    		}
	    	}
	    }
	    //locks sum mutex and pushes a pair of abstract name and its summary
	    pthread_mutex_lock(&mutex_sum);
	    summaries.push_back(make_pair(next_abstract, summary));
	    pthread_mutex_unlock(&mutex_sum);
	}

    pthread_exit(0);


}