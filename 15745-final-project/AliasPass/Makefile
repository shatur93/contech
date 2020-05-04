all: basic intermediate advanced

CXXFLAGS = -rdynamic $(shell llvm-config --cxxflags) -g -O0

FLAGS = $(CXX) -dylib -shared $^ -o $@

basic: AliasBasic/AABasicPass.so
AliasBasic/AABasicPass.so: AliasBasic/AABasicPass.o 
	$(FLAGS)
	# cd AliasBasic && rm -f *.o

intermediate: AliasIntermediate/AAIntermediatePass.so
AliasIntermediate/AAIntermediatePass.so: AliasIntermediate/AAIntermediatePass.o AliasIntermediate/AAEval.o 
	$(FLAGS)
	# cd AliasIntermediate && rm -f *.o

advanced: AliasAdvanced/AAAdvancedPass.so
AliasAdvanced/AAAdvancedPass.so: AliasAdvanced/AAAdvancedPass.o AliasAdvanced/AAEval.o 
	$(FLAGS)
	# cd AliasAdvanced && rm -f *.o


# %.so: %.o 
# 	$(FLAGS)
	

clean:
	cd AliasBasic && rm -f *.o *~ *.so
	cd AliasIntermediate && rm -f *.o *~ *.so
	cd AliasAdvanced && rm -f *.o *~ *.so
