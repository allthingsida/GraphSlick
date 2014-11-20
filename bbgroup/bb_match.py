"""
This module contains IDA specific basic block datatypes and helper functions.

It is written by Ali Rahbar and Ali Pezeshk


10/10/2013 - alirah   - Initial version
10/11/2013 - alipezes -  Added IDABBman subclass of BBMan
                      - Added hash_itype1() and IDABBContext() class
11/07/2013 - eliasb   - Renamed some functions to work with the C adapter
11/08/2013 - alipezes - Fixed serialization issue
08/27/2014 - alirah   - Cleaned up the script and readied it for public release
"""

import idaapi


# ------------------------------------------------------------------------------
import pickle
import cStringIO
from   bb_ida import *
import Queue
from collections import defaultdict
from ordered_set import OrderedSet

# ------------------------------------------------------------------------------
class bbMatcherClass:

	MagicHeader = "--CONTEXT--"
	PathPerNodeHashMarker = "PathPerNodeHash\n"
	PathPerNodeHashFullMarker = "PathPerNodeHashFullMarker\n"
	SizeDicMarker = "Size_Dic\n"
	NodeHashesMarker = "Node_Hashes\n"
	NodeHashMatchesMarker = "Node_Hash_Matches\n"
	
	def __init__(self,func_addr=None):
		self.M={}
		# this one contains paths matched, that have entries only to the head node
		self.pathPerNodeHash=defaultdict(dict)
		# this one contains paths matched, regardless of entries
		self.pathPerNodeHashFull = defaultdict(dict)
		self.normalizedPathPerNodeHash = {}
		self.size_dic={}
		self.sorted_keys=None
		self.G=None
		self.address=None
		self.nodeHashes = defaultdict(dict)
		self.bm=None
		if func_addr!=None:
			self.buildGRaphFromFunc(func_addr)
	
		
	def buildGRaphFromFunc(self,func_addr):
		"""Return a graph object from the function with the hash type 1"""
		self.bm = IDABBMan()
		ok,self.G=self.bm.FromFlowchart(
			func_addr, 
			use_cache=True,
			get_bytes=True,
			get_hash_itype1 =True, 
			get_hash_itype2 =True)
		self.address = func_addr

	def match(self,N1,N2, hashType):
		"""Matches two nodes based on their type1(ordered instruction type hash) hash"""
		if (hashType == 'freq'):
			f1 = get_block_frequency(N1.start, N1.end)
			f2 = get_block_frequency(N2.start, N2.end)
			a, d1 = f1
			b, d2 = f2

			if ( a <= 4 or b <= 4 ):
				coveragePercentage = 50
			elif ( a <= 6 or b <= 6 ):
				coveragePercentage = 60
			elif ( a <= 8 or b <= 8 ):
				coveragePercentage = 75
			else:
				coveragePercentage = 85
			
			b1, b2 = match_block_frequencies(f1, f2, coveragePercentage, 95)
			if (b1 and b2):
				intersection = set.intersection(set(d1.keys()), set(d2.keys()))
				freqHash = hashlib.sha1()
				freqHash.update(intersection.__str__())
				hash = freqHash.hexdigest()
				N1['freq'] = hash
				N2['freq'] = hash
			return b1 and b2
		if ((N1[hashType]==N2[hashType])) :
			return True
		return False
		
	def hashBBMatch(self, hashType):
		"""Creates a dictionary of basic blocks with the hash as the key and matching block numbers as items of a list for that entry"""
		for i in range(0,len(self.G.items())):
			for j in range (i+1,len(self.G.items())):
				if self.match(self.G[i],self.G[j],hashType):
					x=self.G[i][hashType] 
					if self.M.has_key(x):
						if self.M[x].count(j)==0:
							self.M[x]+=[j]
							
					else:
						self.M[x]=[i,j]

	def makeSubgraphSingleEntryPoint(self,path1, path2):
		if (len(path1) != len(path2)):
			quit()
	
		tmp_path1 = list(path1)
		tmp_path2 = list(path2)
		headNode = path1[0]
		nodeRemoved = True
		while nodeRemoved:
			nodeRemoved = False
			for node in tmp_path1:
				for pred in self.G[node].preds:
					if not (pred in tmp_path1) and node != headNode:
						nodeRemoved = True
						break
			if (nodeRemoved):
				try:
					nodeIndex = tmp_path1.index(node)
					tmp_path1.remove(node)
					tmp_path2.remove( tmp_path2[nodeIndex] )
				except:
					quit()

		return OrderedSet(tmp_path1), OrderedSet(tmp_path2)
		
	def findMatchInSuccs(self, node1, Parent2, hashType, visitedNodes2, tmpVisitedNodes2, path2):
		matchedbyHash = False
		for m in self.G[Parent2].succs:
			if (m not in visitedNodes2) and (m !=Parent2) and (m not in path2):
				tmpVisitedNodes2.add(m)
				if (self.match(self.G[node1], self.G[m], hashType)):
					if node1==m:
						continue
					matchedbyHash = True
					break
		return matchedbyHash, m, tmpVisitedNodes2
	

	def findSubGraphs(self):
		"""Find equivalent path from two equivalent nodes
		For each node hash it gets all of the BB and try to build path from each pair of them
		The result is put in a dual dictionary that has the starting node hash as the first key, the path hash as the second key and the equivalent pathS as a list of sets(containing nodes) 
		"""
		matchedPathsWithDifferentLengths = 0
		for i in self.M.keys():
			for z in range(0,len(self.M[i])-1):
				for j in self.M[i][z+1:]:							#pick one from the second node onward
					visited1=set()
					visited2=set()
					q1=Queue.Queue()					#add the first and n node to tmp
					q1.put((self.M[i][z],j))
					path1=OrderedSet()
					path2=OrderedSet()
					path1_bis=OrderedSet()
					path2_bis=OrderedSet()
					path1NodeHashes = {}
					path1.add(self.M[i][z])
					path2.add(j)
					path1Str=''
					path2Str=''
					path1NodeHashes[self.M[i][z]]=self.G[(self.M[i][z])].ctx.hash_itype2
					pathHash1= hashlib.sha1()
					while not q1.empty():			                            # for each matching pair from tmp
						x,y = q1.get(block = False)
						tmp_visited2=set()
						for l in self.G[x].succs :						
							matchedbyHash = False
							if (l not in visited1) and (l !=x) and (l not in path1):
								visited1.add(l)
								tmp_visited2Backup=tmp_visited2   
								hashType = 'hash_itype1'
								matchedbyHash, m, tmp_visited2 = self.findMatchInSuccs( l, y, hashType, visited2, tmp_visited2, path2)

								if not matchedbyHash:
									hashType = 'hash_itype2'
									tmp_visited2= tmp_visited2Backup
									matchedbyHash, m, tmp_visited2 = self.findMatchInSuccs( l, y, hashType, visited2, tmp_visited2, path2)
								
								if not matchedbyHash:
									hashType = 'freq'
									tmp_visited2= tmp_visited2Backup
									matchedbyHash, m, tmp_visited2 = self.findMatchInSuccs( l, y, hashType, visited2, tmp_visited2, path2)

								if matchedbyHash:
									path1NodeHashes[l] = self.G[l][hashType]
									path1.add(l)
									path2.add(m)
									q1.put((l,m))
									visited2.add(m)

						visited2.update(tmp_visited2)
					if (len(path1) != len(path2)):
						matchedPathsWithDifferentLengths += 1
					else:
						path1_bis, path2_bis = self.makeSubgraphSingleEntryPoint(path1, path2) 
				
					if len(path1) >1:
						for kk in path1:
							path1Str+=path1NodeHashes[kk]
							
						pathHash1.update(path1Str)
						a=pathHash1.hexdigest()
						if not(self.pathPerNodeHashFull.has_key(i)) or (not( self.pathPerNodeHashFull[i].has_key(a))):
							self.pathPerNodeHashFull[i][a]=[]

						duplicate1 = False
						duplicate2 = False
					
						listPath1 = list(path1)
						listPath2 = list(path2)
						
						for zz in self.pathPerNodeHashFull[i][a]:
							if listPath1 == zz:
								duplicate1 = True
							if listPath2 == zz:
								duplicate2 = True
								
						if not duplicate1:
							self.pathPerNodeHashFull[i][a].append(list(listPath1))
						if not duplicate2:
							self.pathPerNodeHashFull[i][a].append(list(listPath2))

					if len(path1_bis) >1:
						path1Str = ''
						for kk in path1_bis:
							path1Str+=path1NodeHashes[kk]
							
						pathHash1.update(path1Str)
						a=pathHash1.hexdigest()
						if not(self.pathPerNodeHash.has_key(i)) or (not( self.pathPerNodeHash[i].has_key(a))):
							self.pathPerNodeHash[i][a]=[]

						duplicate1 = False
						duplicate2 = False
					
						listPath1 = list(path1_bis)
						listPath2 = list(path2_bis)
						
						for zz in self.pathPerNodeHash[i][a]:
							if listPath1 == zz:
								duplicate1 = True
							if listPath2 == zz:
								duplicate2 = True
								
						if not duplicate1:
							self.pathPerNodeHash[i][a].append(list(listPath1))
						if not duplicate2:
							self.pathPerNodeHash[i][a].append(list(listPath2))					
		
	def sortByPathLen(self):
		"""It gets the structure created by findSubGraph and creates a dictionary with the path len as the key and the tupple of node hash and path hash as the entry"""
		
		for i in self.pathPerNodeHash:
			for k in self.pathPerNodeHash[i]: 
				a=len(self.pathPerNodeHash[i][k][0])
				if not(self.size_dic.has_key(a)):
					self.size_dic[a]=[]
				self.size_dic[a].append((i,k))
				
		self.sorted_keys=sorted(self.size_dic.keys())

	def subgraphHasExternalJumpsIntoIt(self,subgraph):
		for node in list(subgraph)[1:] :
			if not set(self.G[node].preds).issubset(set(subgraph)):
				return True
		return False
		
	def GetMatchedWellFormedFunctions(self, minFunctionSizeInBlocks = 4, minFunctionHeadSize = 0):
		MovedSubgraph = []
		for i in reversed(sorted(self.size_dic.keys())):
			if i < minFunctionSizeInBlocks :
				break

			for item in self.size_dic[i]:
				x,y=item
				if (not self.normalizedPathPerNodeHash.has_key(x)):
					self.normalizedPathPerNodeHash[x] = {}
				if (not self.normalizedPathPerNodeHash[x].has_key(y)):
					self.normalizedPathPerNodeHash[x][y] = []

				if self.subgraphHasExternalJumpsIntoIt( self.pathPerNodeHash[x][y][0]):
					continue
				for j in self.pathPerNodeHash[x][y]:
					skip=False
					for k in MovedSubgraph:
						if set(j).issubset(set(k)):
							skip=True
							break
					if not skip:
						self.normalizedPathPerNodeHash[x][y].append(j)

				if len(self.normalizedPathPerNodeHash[x][y]) < 2 :
					self.normalizedPathPerNodeHash[x][y] = [] 
				else:
					if ( minFunctionHeadSize > 0 ):
						subgraphStartAddress = self.G[ self.normalizedPathPerNodeHash[x][y][0][0] ].start
						functionHeadBigEnough = True
						for address in range ( subgraphStartAddress, subgraphStartAddress + 8, 2 ):
							if not AddressIsInSubgraph( address, self.normalizedPathPerNodeHash[x][y][0] ):
								functionHeadBigEnough = False
								break
						if not functionHeadBigEnough:
							self.normalizedPathPerNodeHash[x][y] = [] 
							
				MovedSubgraph.extend( self.normalizedPathPerNodeHash[x][y] )

	def AddressIsInSubgraph(self, address, subgraph) :
		for i in subgraph :
			if (self.G[i].start <= address < self.G[i].end) :
				return True
		return False
	
	def FindSimilar(self, nodeList, hashType = 'hash_itype2' ):
		size = len(nodeList)
		headNode = nodeList[0]
		setNodeList = set( nodeList )

		result = []
		
		if ( size == 1 ):
			return self.M[ self.nodeHashes[headNode][hashType] ]
		
		for headNode in nodeList:
			headNodeHash = self.nodeHashes[headNode][hashType]
			for subgraphHash in self.pathPerNodeHashFull[headNodeHash]:
				if size <= len(self.pathPerNodeHashFull[headNodeHash][subgraphHash][0]):
					for match in self.pathPerNodeHashFull[headNodeHash][subgraphHash]:
						if headNode == match[0] and setNodeList.issubset(set(match)):
							# get the subsets from each path that matches the input node list
							matchIndex = {}
							for node in nodeList:
								matchIndex[node] = match.index(node)
							for matchedSubgraph in self.pathPerNodeHashFull[headNodeHash][subgraphHash]:
								subset = []
								for node in nodeList:
									subset.append( matchedSubgraph[matchIndex[node]] )
								if ( not result.__contains__( subset ) ) :
									result.append( subset )
							break
			if len(result) > 0:
				return result
		return []

	def SerializeMatchedInlineFunctions(self, fileName=None ):
		reducedPathPerNodeHash = {}
		if fileName!=None:
			f = open(fileName, 'w')
		else:
			f= StringIO.StringIO()
		for x in self.normalizedPathPerNodeHash:
			reducedPathPerNodeHash[x] = {}
			for y in self.normalizedPathPerNodeHash[x]:
				reducedPathPerNodeHash[x][y] = []
				for i in range(0, len(self.normalizedPathPerNodeHash[x][y])):
					duplicate = False
					for j in reducedPathPerNodeHash[x][y]:
						if self.normalizedPathPerNodeHash[x][y][i].__str__() == j.__str__():
							duplicate = True
							break
					if not duplicate:
						reducedPathPerNodeHash[x][y].append( self.normalizedPathPerNodeHash[x][y][i] )


		
		c = 0
		f.write(bbMatcherClass.MagicHeader + "PATH_INFO\n")
		
		for x in reducedPathPerNodeHash:
			if reducedPathPerNodeHash[x] == {}:
				continue
			for y in reducedPathPerNodeHash[x]:
				if reducedPathPerNodeHash[x][y] == []:
					continue
				f.write ("ID:%s;NODESET:"%(y)),
				count =0
				for i in reducedPathPerNodeHash[x][y]:
					c += 1
					for j in i: # each set
						if j == i[0]:
							f.write ("("),
						f.write ("%d : %x : %x"%(j,self.G[j].start,self.G[j].end)),
						if j != i[len(i)-1]: # last item
							f.write (", "),
						else :
							f.write (")"),
					count +=1
					if count != len(reducedPathPerNodeHash[x][y]): # last one:
						f.write (", "),
					else:
						f.write (";\n")
						
		if fileName!=None:
			result =None
		else:
			result= f.value()
		f.close()
		return result

		
	def SaveState(self,fileName=None):
		if fileName!=None:
			f = open(fileName, 'a')
		else:
			f= StringIO.StringIO()
		result = self.SerializeMatchedInlineFunctions(fileName)
		
		serializedPathPerNodeHash = pickle.dumps( self.pathPerNodeHash )
		serializedPathPerNodeHashFull = pickle.dumps( self.pathPerNodeHash )
		serializedSizeDic = pickle.dumps( self.size_dic )
		serializedNodeHashes = pickle.dumps( self.nodeHashes )
		serializedNodeHashMatches = pickle.dumps (self.M)
		
		f.write(bbMatcherClass.MagicHeader + bbMatcherClass.PathPerNodeHashMarker)
		f.write(serializedPathPerNodeHash)
		f.write(bbMatcherClass.MagicHeader + bbMatcherClass.PathPerNodeHashFullMarker)
		f.write(serializedPathPerNodeHashFull)
		f.write("\n" + bbMatcherClass.MagicHeader + bbMatcherClass.SizeDicMarker)
		f.write(serializedSizeDic)
		f.write("\n" + bbMatcherClass.MagicHeader + bbMatcherClass.NodeHashesMarker)
		f.write(serializedNodeHashes)
		f.write("\n" + bbMatcherClass.MagicHeader + bbMatcherClass.NodeHashMatchesMarker)
		f.write(serializedNodeHashMatches)
		
		if fileName!=None:
			result =None
		else:
			result += f.value()
		f.close()
		return result
		
	def LoadState(self,fileName=None,input=None):
		if fileName!=None:
			f = open(fileName, 'r')
		if input!=None:
			f= StringIO.StringIO(input)
		
		fileContents = f.read()
		
		fileSegments = fileContents.split(bbMatcherClass.MagicHeader)
		
		for segment in fileSegments[1:]  :
			if segment.startswith( bbMatcherClass.PathPerNodeHashMarker ):
				self.pathPerNodeHash = pickle.loads(segment[len(bbMatcherClass.PathPerNodeHashMarker):])
			elif segment.startswith( bbMatcherClass.PathPerNodeHashFullMarker ):
				self.pathPerNodeHashFull = pickle.loads(segment[len( bbMatcherClass.PathPerNodeHashFullMarker):] )
			elif segment.startswith( bbMatcherClass.SizeDicMarker ):
				self.size_dic = pickle.loads(segment[len(bbMatcherClass.SizeDicMarker):])
			elif segment.startswith( bbMatcherClass.NodeHashesMarker ):
				self.nodeHashes = pickle.loads(segment[len( bbMatcherClass.NodeHashesMarker):])
			elif segment.startswith( bbMatcherClass.NodeHashMatchesMarker ):
				self.M = pickle.loads(segment[len( bbMatcherClass.NodeHashMatchesMarker):] )

		f.close()
		
	def Analyze(self,func_addr=None):
		result = []
		if func_addr!=None:
			self.buildGRaphFromFunc(func_addr)
		if self.G !=None:
		# todo: refactor this to get the list from one place
			for hashName in ['hash_itype1', 'hash_itype2']:
				for i in self.G.items():
					self.nodeHashes[i.id][hashName] = self.G[i.id][hashName]
			self.hashBBMatch('hash_itype2')
			self.findSubGraphs()
			self.sortByPathLen()
			self.GetMatchedWellFormedFunctions()
		 

			for x in self.normalizedPathPerNodeHash:
				for y in self.normalizedPathPerNodeHash[x]:
					if self.normalizedPathPerNodeHash[x][y] != []:
						result.append( self.normalizedPathPerNodeHash[x][y] )

		return result

# ------------------------------------------------------------------------------
bbMatcher = bbMatcherClass()
