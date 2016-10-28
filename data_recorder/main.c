/**
 * section: Spec NeXuS data writer.
 * synopsis: Prints NeXuS file from spec shared memory arrays.
 * purpose: Parse an XML defintion file that templates the NeXus file output,
		copy data from spec shared memory required to write file,
		write the file, and update frames of data. 
 * usage: ./spec_data NXsgm.xml
 * author: Zachary Arthur
 * copy: GNU General Public License
 */
#include <stdio.h>
#include <stdint.h>
#include <libxml/xpath.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <string.h>
#include <time.h>
#include "sps.h"
#include "napi.h"

#define STRMAX 255
#ifdef LIBXML_TREE_ENABLED


/*
* sgm_ndAttr structure contains the necessary information
* to write a field from a spec shared memory array to file
*/
typedef struct {
	char* path;
	char* type;
	char* name;
	char* spec;
	char* value;
	int* index;
	int* rank;
	int* error;
} sgm_ndAttr;

/*
* Declaration of functions used by this application. 
*/
char *getGroupPath(char *spec_version, xmlNode *group_node);
void *getNexusNodeProp(char *spec_version, xmlNode *group_node);
int writeNexusHead(xmlDocPtr doc, char *spec_name, NXhandle fileid);
int writeNexusData(xmlDocPtr xmlDoc, char *spec_name, NXhandle fileid);
int specDataWrite(char *spec_version, sgm_ndAttr *var_name, NXhandle *file_id, int mode);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
void freeSGMnode(sgm_ndAttr *node_prop);
int updateBuffer(char *spec_version);

/*
* Function below takes in a group/field node, reads in the properties list and populates
* the values to a sgm_ndAttr variable with the result.
*/
void *getNexusNodeProp(char *spec_version, xmlNode *group_node){
	xmlAttr *cur_attr;
	xmlChar *attr;
	int attr_ln;
	char *scan = NULL;
	int spec_asgn[2] = {1,1};
	int type_nm_asgn[2] = {1,1};
	sgm_ndAttr *nexusNode = NULL;

	//Allocate memory for components of sgm_ndAttr variable.
	nexusNode = (sgm_ndAttr*)malloc(sizeof(sgm_ndAttr));
	if(nexusNode != NULL){
		nexusNode->path = getGroupPath(spec_version, group_node);
		nexusNode->type = (char*)malloc(10*sizeof(char));
		nexusNode->name = (char*)malloc(10*sizeof(char));
		nexusNode->spec = (char*)malloc(10*sizeof(char));
		nexusNode->value = (char*)malloc(10*sizeof(char));
		nexusNode->index = (int*)malloc(sizeof(int));
		nexusNode->rank = (int*)malloc(sizeof(int));
		nexusNode->error = (int*)malloc(sizeof(int));
	}
	else{
		fprintf(stderr, "Failure allocating nexusNode in getNexusNodeProp");
		*(nexusNode->error) = 1;
		return nexusNode;
	}
	//Initialize all values of nexusNode, to be used later to determine file I/O mode.
	strncpy(nexusNode->type, "\0", 1);
	strncpy(nexusNode->name, "\0", 1);
	strncpy(nexusNode->spec, "\0", 1);
	strncpy(nexusNode->value, "\0", 1);
	*(nexusNode->rank) = -1;
	*(nexusNode->index) = -1;
	*(nexusNode->error) = 0;	
	for (cur_attr = group_node->properties; cur_attr; cur_attr = cur_attr->next){
		if (cur_attr->type == XML_ATTRIBUTE_NODE) {
			//Each group node must contain an NX_class and a name, if it doesn't set error flag.
			if(strncmp(cur_attr->name, "type", 4) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no object type, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				type_nm_asgn[0] = 0;
				attr_ln = strlen(attr) + 5;
				nexusNode->type = realloc(nexusNode->type, attr_ln);
				strncpy(nexusNode->type,attr, attr_ln);
				
			}
			if(strncmp(cur_attr->name, "name", 4) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no object name, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				type_nm_asgn[1] = 0;
				attr_ln = strlen(attr) + 5;
				nexusNode->name = realloc(nexusNode->name, attr_ln);
				strncpy(nexusNode->name,attr, attr_ln);
				if(strcmp(attr, "entry")== 0){
					scan = SPS_GetEnvStr(spec_version, "core_shm", "scan");
					strcat(nexusNode->name, scan);
				}
			}
			//Don't have to assign spec, set spec assigned flag to 1 to guarantee and index.
		    	else if(strncmp(cur_attr->name, "spec", 5) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no value to spec, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				else{
					spec_asgn[0] = 0;
					attr_ln = strlen(attr) + 5;
					nexusNode->spec = realloc(nexusNode->spec, attr_ln);
					strncpy(nexusNode->spec,attr, attr_ln);
				}
		   	}
			//If present, set the index unit, set index(spec) assigned flag to 1.
		    	else if(strncmp(cur_attr->name, "index", 5) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no value to index, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				else{
					spec_asgn[1] = 0;
					attr_ln = strlen(attr) + 5;
					char *tempstring = (char*)malloc(100*sizeof(char));
					strncpy(tempstring, attr, 100);
					*(nexusNode->index) = atoi(tempstring);
					free(tempstring);
				}
		   	}
			//If present assign value a value. This is incompatible with a postive value of rank. 
		    	else if(strncmp(cur_attr->name, "value", 5) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no value to value, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				else{
					attr_ln = strlen(attr) + 5;
					nexusNode->value = realloc(nexusNode->value, attr_ln);
					strncpy(nexusNode->value,attr, attr_ln);
				}
		   	}
			//If rank assigned, get value.  A non negative rank will indicate that the data set will be recorded as frames.
		    	else if(strncmp(cur_attr->name, "rank", 5) == 0){
 				attr = xmlNodeGetContent((xmlNode*)cur_attr);
				if(attr == NULL) {
					fprintf(stderr, "%s assigned no value to rank, but made declaration.\n", nexusNode->path);
					*(nexusNode->error) = 1;
					return nexusNode;
				}
				else{
					attr_ln = strlen(attr) + 5;
					char *tempstring = (char*)malloc(100*sizeof(char));
					strncpy(tempstring, attr, 100);
					*(nexusNode->rank) = atoi(tempstring);
					free(tempstring);
				}
		   	}
		}
	}
	if(type_nm_asgn[0] || type_nm_asgn[1]){
		fprintf(stderr, "%s is without assigned NX_Class or name.\n", nexusNode->path);
		*(nexusNode->error) = 1;
	}
	if(spec_asgn[0] != spec_asgn[1]){
		fprintf(stderr, "Spec shared memory assigned without corresponding index. \n");
		*(nexusNode->error) = 1;
	}
	return nexusNode;
	
}

/*
* Function getGroupPath does just that. 
*
*/
char *getGroupPath(char *spec_version, xmlNode *group_node){
	xmlNode *cur_node;
	xmlAttr *cur_attr;
	xmlChar *attr;
	char groupName[STRMAX];
	char *pathName, *tmp, *scan_n;
	pathName = (char*)malloc(10);
	tmp = (char*)malloc(10);
	scan_n = (char*)malloc(STRMAX);
	strncpy(pathName, "\0", 1);
	strncpy(scan_n, "\0", 1);
	size_t pathSize;
	for(cur_node = group_node->parent; cur_node; cur_node = cur_node->parent){
		if(cur_node->type == XML_ELEMENT_NODE){
			for(cur_attr = cur_node->properties; cur_attr; cur_attr = cur_attr->next){
				if(cur_attr->type == XML_ATTRIBUTE_NODE){
					if(strncmp(cur_attr->name, "name", 4) == 0){
						attr = xmlNodeGetContent((xmlNode*)cur_attr);
						if(strcmp(attr, "entry") == 0){
							strncpy(scan_n, (SPS_GetEnvStr(spec_version, "core_shm", "scan")), STRMAX);
							
							if(strcmp(scan_n, "\0") != 0) sprintf(groupName, "/%s%s", (char *)attr, 
															scan_n);
							else fprintf(stderr, "There was a problem creating entry group path. \n");
						}
						else{		
							sprintf(groupName, "/%s", (char *)attr);
						}
						pathSize = strlen(groupName) + strlen(pathName) + 10;
						pathName = (char *)realloc(pathName, pathSize);
						tmp = (char*)realloc(tmp, pathSize);
						strcpy(tmp, groupName);
						strcat(tmp, pathName);
						strcpy(pathName, tmp); 	
					}
				}
			}
		}
	}
	//printf("%s\n", pathName);
	free(tmp);
	return pathName;
}

int isSignal(char *spec_version, sgm_ndAttr *nd_prop){
	char *signal;
	signal = (char *)malloc(STRMAX*sizeof(char));
	if(signal == NULL){
		fprintf(stderr, "Error initiallizing signal attribute. \n");
		return 0;
	}
	strncpy(signal, (SPS_GetEnvStr(spec_version, "core_shm", "signal")), STRMAX);
	if(!strcmp(signal, nd_prop->name)){
		 free(signal);
		 return 1;
	}
	else{
		 free(signal);
		 return 0;
	}
}

int isAxes (char *spec_version, sgm_ndAttr *nd_prop){
	char *axes;
	char delim[2] = ",";
	char *token;
	axes = (char*)malloc(STRMAX*sizeof(char));
	if(axes == NULL){
		fprintf(stderr, "Error initializing axes parse. \n");
		return 0;
	}
	strncpy(axes, (SPS_GetEnvStr(spec_version, "core_shm", "axes")), STRMAX);
	
	token = strtok(axes, delim);

	while( token != NULL){
		if(strcmp(token, nd_prop->name) == 0){
			free(axes);
			return 1;
		}
		token = strtok(NULL, delim);
	}
	if(axes != NULL) free(axes);
	return 0;
}

int createNexusGroups(xmlDocPtr xmlDoc,char *spec, NXhandle fileid){
	xmlNode *cur_node;
        xmlNodeSetPtr nodeset;
        xmlXPathObjectPtr result;	
	sgm_ndAttr *nodeprop = (sgm_ndAttr*)malloc(sizeof(sgm_ndAttr));
	int i;
	
	result = getnodeset(xmlDoc, "//group");
	if(result){
		nodeset = result->nodesetval;
		for (i=0; i < nodeset->nodeNr; i++) {
			cur_node = nodeset->nodeTab[i];
			nodeprop = getNexusNodeProp(spec,cur_node);	
			
			if(i == 0){
				if(NXmakegroup(fileid, nodeprop->name, nodeprop->type) != NX_OK)return 1;
				if(NXopengroup(fileid, nodeprop->name, nodeprop->type) != NX_OK)return 1;
				if(NXputattr(fileid, "default", "data",strlen("data"), NX_CHAR) != NX_OK) return 1; 
			}
			else{
				if(NXopengrouppath(fileid, nodeprop->path) != NX_OK) return 1;
				if(NXmakegroup(fileid, nodeprop->name, nodeprop->type) != NX_OK)return 1;
				if(NXopengroup(fileid, nodeprop->name, nodeprop->type) != NX_OK)return 1;
			}
		if(NXclosegroup(fileid) != NX_OK) return 1;
		freeSGMnode(nodeprop);
		}
	}
return 0;
}

int createNexusData(xmlDocPtr xmlDoc, char *spec, NXhandle fileid){
	xmlNode *cur_node;
        xmlNodeSetPtr nodeset;
        xmlXPathObjectPtr result;	
	NXlink dlink;
	char **NX_axes = (char**)malloc(2*sizeof(char));
	sgm_ndAttr *nd_prop = (sgm_ndAttr*)malloc(sizeof(sgm_ndAttr));
	int i, var_ln, one = 1;
	int tempint, bool, axis_ct = 0;
	double *tempdouble;
	char *NX_data_path = (char*)malloc(STRMAX*sizeof(char));
	char *scan = (char*)malloc(10*sizeof(char));

	//Initialize filename and NXdata path variables.	
	if(strncpy(scan, (SPS_GetEnvStr(spec, "core_shm", "scan")), 10) > 0){
		strncpy(NX_data_path,"/entry", STRMAX);
	       	strcat(NX_data_path, scan);	
		strcat(NX_data_path, "/data");
	}
	else{
		fprintf(stderr, "There was a problem initializing the NXdata path.\n");
		return 1;
	}

	result = getnodeset(xmlDoc, "//field");
	if(result){
		nodeset = result->nodesetval;
		for (i=0; i < nodeset->nodeNr; i++) {
			cur_node = nodeset->nodeTab[i];
			nd_prop = getNexusNodeProp(spec,cur_node);	
			if(*(nd_prop->error) !=1){
				if (NXopengrouppath (fileid, nd_prop->path) != NX_OK) return 1;
				// Place header information for XML nodes which contain the value flag.
				if((strcmp(nd_prop->spec, "\0") == 0) && (strcmp(nd_prop->value, "\0") > 0) &&(*(nd_prop->rank) < 0)){
					// Save method for values with type "NX_BOOLEAN".
					if(strcmp(nd_prop->type, "NX_BOOLEAN") == 0){
						if(strcmp(nd_prop->value, "TRUE") == 0) bool = 1;
						if (NXmakedata(fileid, nd_prop->name, NX_UINT32, one, &one) != NX_OK) return 1;
     						if (NXopendata(fileid, nd_prop->name) != NX_OK)return 1;
        					if (NXputdata(fileid, &bool) != NX_OK)return 1;
						if (NXclosedata (fileid) != NX_OK) return 1;
					} 
						// Save method for values with type "NX_CHAR".
					else if(strcmp(nd_prop->type, "NX_CHAR") == 0){
						var_ln = strlen(nd_prop->value);
						if(NXmakedata(fileid, nd_prop->name, NX_CHAR, one, &var_ln) != NX_OK) return 1;
						if(NXopendata(fileid, nd_prop->name) != NX_OK)return 1;
						if(NXputdata(fileid, nd_prop->value) != NX_OK)return 1;
						if (NXclosedata (fileid) != NX_OK) return 1;
					} 
					else if(strcmp(nd_prop->type, "NX_FLOAT") == 0) {
						if(NXmakedata(fileid, nd_prop->name, NX_FLOAT64, one, &one) != NX_OK) return 1;
						if(NXopendata(fileid, nd_prop->name) != NX_OK) return 1;
							tempdouble = (double*)malloc(sizeof(double));
							*tempdouble = atof(nd_prop->value);
							if(NXputdata(fileid, tempdouble) != NX_OK) return 1;
						if (NXclosedata (fileid) != NX_OK) return 1;
						if(tempdouble != NULL)free(tempdouble);
					}
					else if(strcmp(nd_prop->type, "NX_INT") == 0){
						char *tempstring = (char*)malloc(STRMAX*sizeof(char));
						strncpy(tempstring, nd_prop->value, 100);
						tempint = atoi(tempstring);
						free(tempstring);
						if(NXmakedata(fileid, nd_prop->name, NX_INT32, one, &one) != NX_OK) return 1;
						if(NXopendata(fileid, nd_prop->name) != NX_OK) return 1;
							if(NXputdata(fileid, &tempint) != NX_OK)return 1;
						if (NXclosedata (fileid) != NX_OK) return 1;
					} 
				}
			// Place header information for XML nodes which rely upon spec shared memory. 
			// Also link in fields with the signal/axis settings from spec shared memory. 
				else if((strcmp(nd_prop->spec, "\0") >= 0) && (*(nd_prop->rank) < 0)){
					//Write the static spec data from shared memory array.
					if(specDataWrite(spec, nd_prop, &fileid, 0)){
						fprintf(stderr, "Trouble writing static spec field: %s.\n", nd_prop->path);
					//	return 1;
					}
				}
				else if ((strcmp(nd_prop->spec, "\0") >= 0) && (*(nd_prop->rank) > 0)){
				//Allocate unlimited NXdata of rank nd_prop->rank. 
					if(!specDataWrite(spec, nd_prop, &fileid, 1)){
						if(isSignal(spec, nd_prop)){
							//link field to /entry/data
							tempint = 1;
							if (NXopendata(fileid, nd_prop->name) != NX_OK) return 1;
							
							if (NXgetdataID (fileid, &dlink) != NX_OK) return 1; 
					//		if (NXputattr(fileid, "signal", &tempint, 1, NX_INT32) != NX_OK) return 1;
							if (NXclosedata(fileid) != NX_OK) return 1;
							if(NXclosegroup(fileid) != NX_OK) return 1;
							//add attribute to /entry/data
							if(NXopengrouppath(fileid, NX_data_path) != NX_OK)return 1;
							if(NXmakelink(fileid, &dlink) != NX_OK) return 1;
							if(NXputattr(fileid, "signal", nd_prop->name, strlen(nd_prop->name), NX_CHAR) != NX_OK) return 1;
						
						}
						else if(isAxes(spec, nd_prop)){
							//link filed to /entry/data/
							if (NXopendata(fileid, nd_prop->name) != NX_OK) return 1;
							if (NXgetdataID (fileid, &dlink) != NX_OK) return 1; 
							if(NXclosedata(fileid) != NX_OK) return 1;							
							if(NXclosegroup(fileid) != NX_OK) return 1;
							if(NXopengrouppath(fileid, NX_data_path) != NX_OK) return 1;
							if(NXmakelink(fileid, &dlink) != NX_OK) return 1;
							//Record axis name for future writing of axes attribute.
							axis_ct++;
							NX_axes = realloc(NX_axes, axis_ct*sizeof(char*));
							NX_axes[axis_ct-1] = malloc(STRMAX*sizeof(char));
							strncpy(NX_axes[axis_ct -1], nd_prop->name, STRMAX);
						}
					}
					else{
						fprintf(stderr, "Trouble writing dynamic spec field: %s/%s.\n", nd_prop->path, nd_prop->name);
					//	return 1;
					}
				}
			NXclosegroup(fileid);
			freeSGMnode(nd_prop);	
			}
		}	
		//Write the axes and signal attributes to /entry/data/
		char *tempchar = (char*)malloc(STRMAX*sizeof(char));
		char strings[axis_ct][STRMAX];	
		if(NXopengrouppath(fileid, NX_data_path) != NX_OK) return 1;
		for(i=0; i < axis_ct; i++){
			one += i;
			strncpy(strings[i], NX_axes[i], STRMAX);
			strncpy(tempchar, NX_axes[i], STRMAX);
			strcat(tempchar, "_indices");
			if(NXputattr(fileid, tempchar, &one, 1, NX_INT32) != NX_OK) return 1;
			if(NX_axes[i] != NULL) free(NX_axes[i]);
		}
		if(tempchar != NULL) free(tempchar);
		if(NXputattr(fileid, "axes", &strings, sizeof(strings), NX_CHAR) != NX_OK)return 1;
		if(NXclosegroup(fileid) != NX_OK) return 1;
	}

return 0;
}


int createNexusAttr(xmlDocPtr xmlDoc,char *spec, NXhandle fileid){
	xmlNode *cur_node;
        xmlNodeSetPtr nodeset;
        xmlXPathObjectPtr result;	
	sgm_ndAttr *nodeprop = (sgm_ndAttr*)malloc(sizeof(sgm_ndAttr));
	int i, tempint, bool, var_ln, one = 1;
	bool = 0;
	double *tempdouble;
	
	result = getnodeset(xmlDoc, "//attribute");
	if(result){
		nodeset = result->nodesetval;
		for (i=0; i < nodeset->nodeNr; i++) {
			cur_node = nodeset->nodeTab[i];
			nodeprop = getNexusNodeProp(spec,cur_node);	
			if(*(nodeprop->error) !=1){
				NXopenpath(fileid, nodeprop->path);
				if((strcmp(nodeprop->spec, "\0") == 0) && (strcmp(nodeprop->value, "\0") > 0)){
					// Save method for attribute values with type "NX_BOOLEAN".
					if(strcmp(nodeprop->type, "NX_BOOLEAN") == 0){
						if (strcmp(nodeprop->value, "TRUE") == 0) bool = 1;	
						if (NXputattr(fileid, nodeprop->name, &bool, one, NX_UINT32) != NX_OK) return 1;
					} 
					// Save method for attribute values with type "NX_CHAR".
					else if(strcmp(nodeprop->type, "NX_CHAR") == 0){
						var_ln = strlen(nodeprop->value);
						if(NXputattr(fileid, nodeprop->name, nodeprop->value, var_ln, NX_CHAR) != NX_OK) return 1;
					} 
					// Save method for attribute values with type "NX_FLOAT"
					else if(strcmp(nodeprop->type, "NX_FLOAT") == 0) {
						tempdouble = (double*)malloc(sizeof(double));
						*tempdouble = atof(nodeprop->value);
						if(NXputattr(fileid, nodeprop->name, tempdouble, sizeof(double), NX_FLOAT64) != NX_OK) return 1;
						free(tempdouble);
					}
					// Save method for attribute values with type "NX_INT"
					else if(strcmp(nodeprop->type, "NX_INT") == 0){
						char *tempstring = (char*)malloc(STRMAX*sizeof(char));
						strncpy(tempstring, nodeprop->value, 100);
						tempint = atoi(tempstring);
						free(tempstring);
						if(NXputattr(fileid, nodeprop->name, &tempint, sizeof(int), NX_INT32) != NX_OK) return 1;
						} 
				}
				else if(strcmp(nodeprop->spec, "\0") >= 0){
					if(specDataWrite(spec, nodeprop, &fileid, 2)){
						fprintf(stderr, "Trouble adding attribute to %s.\n", nodeprop->path);			
					//	return 1;
					}
				}	
			}
		NXclosegroup(fileid);
		freeSGMnode(nodeprop);	
		}
	}
return 0;
}
/**
 * writeNexusHead:
 * @a_node: the initial xml node to consider.
 *
 * Prints the names of the all the xml elements
 * that are siblings or children of a given xml node.
 */
int writeNexusHead(xmlDocPtr xmlDoc, char *spec_name, NXhandle fileid){
		
	if(createNexusGroups(xmlDoc, spec_name, fileid) == 1) return 1;
	if(createNexusData(xmlDoc, spec_name, fileid) == 1) return 1;
	if(createNexusAttr(xmlDoc, spec_name, fileid) == 1) return 1;
	printf("Done writing header. \n");
	
	return 0;
}

/*
* freeSGMnode frees an struct of type sgm_ndAttr.
*/

void freeSGMnode(sgm_ndAttr *node_prop){
	if(node_prop != NULL){
		if(node_prop->error != NULL) free(node_prop->error);
		if(node_prop->rank != NULL) free(node_prop->rank);
		if(node_prop->value != NULL) free(node_prop->value);
		if(node_prop->index != NULL) free(node_prop->index);
		if(node_prop->spec != NULL) free(node_prop->spec);
		if(node_prop->name != NULL) free(node_prop->name);
		if(node_prop->type != NULL) free(node_prop->type);
		if(node_prop != NULL) free(node_prop);
	}
}

/*
 * Function updates frames of SPEC data with the XML
 * attribute "rank > 0" using mode 4 of the specDataWrite 
 * function to put slabs into the NeXuS data file.
 * Returns 1 on error, 0 on success. 
 */
int writeNexusData(xmlDocPtr xmlDoc, char *spec_name, NXhandle fileid){
	xmlNode *cur_node;
        xmlNodeSetPtr nodeset;
        xmlXPathObjectPtr result;	
	sgm_ndAttr *nd_prop = (sgm_ndAttr*)malloc(sizeof(sgm_ndAttr));
	int i, one = 1;
	
	result = getnodeset(xmlDoc, "//field[@rank]");
	if(result){
		nodeset = result->nodesetval;
		for (i=0; i < nodeset->nodeNr; i++) {
			cur_node = nodeset->nodeTab[i];
			nd_prop = getNexusNodeProp(spec_name,cur_node);	
			if(nd_prop != NULL && *(nd_prop->rank) > 0){
				if(NXopenpath(fileid, nd_prop->path) != NX_OK) return 1;
				if(specDataWrite(spec_name, nd_prop, &fileid, 3)){
					fprintf(stderr, "Trouble adding data to %s/%s", nd_prop->path, nd_prop->name);
					//return 1;
				}
				if(NXclosedata(fileid) != NX_OK) return 1;
			}
		}
	}
	freeSGMnode(nd_prop);
	NXclose(fileid);
	return 0;
}
/*
* Function creates buffer, and copies SPS row data array to shared memory
* taking care of data type compatibility and size constraints. 
* Returns: void * pointer on success
*	   NULL on error
*/
void *getSpecRow(char *spec, sgm_ndAttr *nodeprop, int rows, int cols, int type, int *copied){
	int size;
	void *temp;
	size = cols*rows;
	if(type == 0) temp = (double*)malloc(size*sizeof(double));
	if(type == 1) temp = (float*)malloc(size*sizeof(double));
	if(type == 2) temp = (int*)malloc(size*sizeof(int));
	if(type == 3) temp = (unsigned int*)malloc(size*sizeof(int));
	if(type == 4) temp = (short*)malloc(size*sizeof(short));
	if(type == 5) temp = (unsigned short*)malloc(size*sizeof(short));
	if(type == 6) temp = (char*)malloc(size*sizeof(char));
	if(type == 7) temp = (unsigned char*)malloc(size*sizeof(char));
	if(type == 8){
		temp = (char*)malloc(cols*sizeof(char));
		strncpy(temp, "\0", cols);
	}
	if(type == 9) temp = (long*)malloc(size*sizeof(long));
	if(type == 10) temp = (unsigned long*)malloc(size*sizeof(long));
	if(type == 11) temp = (long long*)malloc(size*sizeof(long long));
	if(type == 12) temp = (unsigned long long*)malloc(size*sizeof(long long));

	if(SPS_CopyRowFromShared(spec, nodeprop->spec, temp, type, *(nodeprop->index), cols, copied)){
		return NULL;
	}
	return temp;	
}


/*
 * Function creates buffer, and copies SPS col data array to shared memory
 * taking care of data type compatibility and size constraints. 
 * Returns: void * pointer on success
 *	   NULL on error
 */
void *getSpecCol(char *spec, sgm_ndAttr *nodeprop, int rows, int cols, int type, int *copied){
	void *temp;
	int size;
	size = cols*rows;
	if(type == 0) temp = (double*)malloc(size*sizeof(double));
	if(type == 1) temp = (float*)malloc(size*sizeof(double));
	if(type == 2) temp = (int*)malloc(size*sizeof(int));
	if(type == 3) temp = (unsigned int*)malloc(size*sizeof(int));
	if(type == 4) temp = (short*)malloc(size*sizeof(short));
	if(type == 5) temp = (unsigned short*)malloc(size*sizeof(short));
	if(type == 6) temp = (char*)malloc(size*sizeof(char));
	if(type == 7) temp = (unsigned char*)malloc(size*sizeof(char));
	if(type == 8){
		 temp = (char*)malloc(size*sizeof(char));
		 strcpy(temp, "\0");
	}
	if(type == 9) temp = (long*)malloc(size*sizeof(long));
	if(type == 10) temp = (unsigned long*)malloc(size*sizeof(long));
	if(type == 11) temp = (long long*)malloc(size*sizeof(long long));
	if(type == 12) temp = (unsigned long long*)malloc(size*sizeof(long long));

	if(SPS_CopyColFromShared(spec, nodeprop->spec, temp, type, *(nodeprop->index), rows, copied)){
		return NULL;
	}
	return temp;	
}

/*
 * Function creates buffer, and copies SPS col data array to shared memory
 * taking care of data type compatibility and size constraints. 
 * Returns: void * pointer on success
 *	   NULL on error
 */
void *getSpec2D(char *spec, sgm_ndAttr *nodeprop, int rows, int cols, int type){
	void *temp;
	int size;
	size = cols*rows;
	if(type == 0) temp = (double*)malloc(size*sizeof(double));
	if(type == 1) temp = (float*)malloc(size*sizeof(double));
	if(type == 2) temp = (int*)malloc(size*sizeof(int));
	if(type == 3) temp = (unsigned int*)malloc(size*sizeof(int));
	if(type == 4) temp = (short*)malloc(size*sizeof(short));
	if(type == 5) temp = (unsigned short*)malloc(size*sizeof(short));
	if(type == 6) temp = (char*)malloc(size*sizeof(char));
	if(type == 7) temp = (unsigned char*)malloc(size*sizeof(char));
	if(type == 8){
		 temp = (char*)malloc(size*sizeof(char));
		 strcpy(temp, "\0");
	}
	if(type == 9) temp = (long*)malloc(size*sizeof(long));
	if(type == 10) temp = (unsigned long*)malloc(size*sizeof(long));
	if(type == 11) temp = (long long*)malloc(size*sizeof(long long));
	if(type == 12) temp = (unsigned long long*)malloc(size*sizeof(long long));

	if(!SPS_CopyFromShared(spec, nodeprop->spec, temp, type, sizeof(temp))){
		return NULL;
	}
	return temp;	
}
/*
 * Function maps SPS variable types to NX variable types
 * represented by an integer value. 
 * Input:  SPS type.
 * Output: NX type.
 */

int nxType(int type){
	int nxt;
	if(type == 0) nxt = NX_FLOAT64;
	if(type == 1) nxt = NX_FLOAT32;
	if(type == 2) nxt = NX_INT32;
	if(type == 3) nxt = NX_UINT32;
	if(type == 4) nxt = NX_INT16;
	if(type == 5) nxt = NX_UINT16;
	if(type == 6) nxt = NX_CHAR;
	if(type == 7) nxt = NX_CHAR;
	if(type == 8) nxt = NX_CHAR;
	if(type == 9) nxt = NX_INT32;
	if(type == 10) nxt = NX_UINT32;
	if(type == 11) nxt = NX_INT64;
	if(type == 12) nxt = NX_UINT64;

	return nxt;
}

/*
 * Program for fetching spec shared memory data indicated by NXsgm.xml files. Function 
 * looks at spec data type, copies it into a buffer of the correct type and then writes
 * it to a NeXuS file. 
 *
 * specDataWrite(char* spec_name, sgm_ndAttr *node_properties, NXhandle *fileid, int Mode)
 *	spec_name: the pointer to the SPS shared memory instance of spec
 *	node_properties: pointer to struct type sgm_ndAttr containing all node properties
 *			from the NXsgm.xml file. 
 *	Fileid:  NXhandle for writing data to NeXuS file.
 *	Mode: int 0,1,2,3
 *		|-> 0 for single write of static data, denoted rank < 0 in NXsgm.xml
 *		|-> 1 to initialized a data point that will be updated dynamically later.
 *		|-> 2 for single write of static attribute.
 *		|-> 3 update mode for data previously initialized using mode 1. 
 *	Returns: 0 on success
 * 		 1 on error.
 */
int firstUpdate = 0;

int specDataWrite(char *spec_version, sgm_ndAttr *var_name, NXhandle *file_id, int mode){
	int nxt, i, buf, one = 1;
	int rows, cols, flag, size, type;
	void *temparray;
	int slab_start[2]={2,2}; 
	int slab_size[2]={2,2}; 
	int NXrank, NXtype;
	int NXdim[2]={0,0};
	int *copied = (int*)malloc(sizeof(int));
	int unlimited_dim[1]={NX_UNLIMITED};
	int unlimited_dims[2] = {NX_UNLIMITED, 1};

	buf = updateBuffer(spec_version);
	if(SPS_GetArrayInfo(spec_version, var_name->spec, &rows, &cols, &type, &flag) != 0){
		return 1;
	}
	nxt = nxType(type);
	//Determine the shape of the array, 2D arrays in spec are indexed by row, 1D by column.
	if((*(var_name->rank) <= 1) && (type != 8)){
		temparray = getSpecCol(spec_version, var_name, buf, 1, type, copied);
		if(temparray == NULL) return 1; 
	}
	else if(type == 8){
		temparray = getSpecRow(spec_version, var_name, 1, cols, type, copied);
		if(temparray == NULL) return 1;
	}
	else if(*(var_name->rank) > 1){
		temparray = getSpec2D(spec_version, var_name, buf, cols, type);
		if(temparray == NULL) return 1;
	}
	if(mode == 0){
		if(type != 8){
			if(NXmakedata(*(file_id), var_name->name, nxt, one, &rows) != NX_OK) return 1;
		}
		else{
		 	if(NXmakedata(*(file_id), var_name->name, nxt, one, &cols) != NX_OK) return 1;
		}
		if(NXopendata (*(file_id), var_name->name) != NX_OK) return 1;
			if(NXputdata (*(file_id), temparray) != NX_OK) return 1;
	}
	else if(mode == 1){
		if(*(var_name->rank) <= 1){
			if(NXmakedata (*(file_id), var_name->name, nxt, 1, unlimited_dim) != NX_OK) return 1;
		}
		else if (*(var_name->rank) > 1){
			unlimited_dims[0] = NX_UNLIMITED;
			unlimited_dims[1] = *(var_name->rank);
			if(NXmakedata (*(file_id), var_name->name, nxt, 2, unlimited_dims) != NX_OK) return 1;
		}
	}
	else if(mode == 2){
		//NXputattr can only handle arrays of type CHAR and INT, keep FLOATS as single #.
		if(type == 0 || type == 1){
			if(NXputattr(*(file_id), var_name->name, temparray, 1, nxt) != NX_OK) return 1;
		}
		else{
			if(NXputattr(*(file_id), var_name->name, temparray, sizeof(temparray), nxt) != NX_OK) return 1;
		}
	}
	else if(mode == 3){
		size = rows*cols;
		if(NXopenpath(*(file_id), var_name->path) != NX_OK) return 1;
		if(NXopendata(*(file_id), var_name->name) != NX_OK) return 1;
		if(NXgetinfo(*(file_id), &NXrank, NXdim, &NXtype) != NX_OK) return 1;
		if(*(var_name->rank) <= 1){
			slab_start[0] = NXdim[0];
			slab_start[1] = 0;
			slab_size[0] = *(copied);
			slab_size[1] = 1;
			if(firstUpdate == 0 && NXdim[1] == 1){
				 slab_start[0] = 0;
				 firstUpdate == 1;
			}
			else if(firstUpdate == 1 && NXdim[1] == 1) firstUpdate = 0;
			if(NXputslab(*(file_id), temparray, slab_start, slab_size) != NX_OK) return 1;
		}
		else if (*(var_name->rank) > 1){
			slab_start[0] = NXdim[0];
			slab_start[1] = 0;
			slab_size[0] = buf;
			slab_size[1] = cols;
			if(firstUpdate == 0 && NXdim[1] == 1){
				 slab_start[0] = 0;
				 firstUpdate == 1;
			}
			else if(firstUpdate == 1 && NXdim[1] == 1) firstUpdate = 0;
			if(NXputslab(*(file_id), temparray, slab_start, slab_size) != NX_OK) return 1;
		}
	}
	
	if(temparray != NULL) free(temparray);
	return 0;
}

/*
* Function retrieves an XPathObjectPtr which gives a
* linked list from XML file which is then used
* to navigate the NXdata file efficiently.
* Returns: Pointer on success
* 	   NULL on error
*/
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath){

        xmlXPathContextPtr context;
        xmlXPathObjectPtr result;

        context = xmlXPathNewContext(doc);
        if (context == NULL) {
                fprintf(stderr, "Error in xmlXPathNewContext.\n");
                return NULL;
        }
        result = xmlXPathEvalExpression(xpath, context);
        xmlXPathFreeContext(context);
        if (result == NULL) {
                fprintf(stderr, "Error in xmlXPathEvalExpression.\n");
                return NULL;
        }
        if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
                xmlXPathFreeObject(result);
                fprintf(stderr, "No result from xPath search.\n");
                return NULL;
        }
        return result;
}

int updateBuffer(char *spec_name){
	int buffer;
	char *buf = (char*)malloc(STRMAX*sizeof(char));
	if(strncpy(buf, (SPS_GetEnvStr(spec_name, "core_shm", "buffer")), STRMAX) != NULL){
		buffer = atoi(buf);
	}
	else{
		buffer = 1;
	}
	if(buf != NULL) free(buf);
	return buffer;
}

/**
 * Simple switch controlled by core_shm status register. 
 * Program reads in XML template from argv[2] which contains 
 * the beamline specific template of writing data to a NeXuS
 * formatted HDF5 file.
 * 
 * Switchs are:
 * sw=0, Create new HDF5 file.
 * sw=1, Reopen the file.
 * sw=2, Close the file.
 * sw=3, Write the header to NeXuS file, and initialize data.
 * sw=4, Append and write the data of rank > 0 from xml file.
 * sw=100, kill the application. 
 */
int main(int argc, char **argv){
    	xmlDocPtr doc = NULL;
	xmlXPathObjectPtr xpathObj = NULL; 
    	struct timespec sleep_dur;
	int wN_stat = 1;
	int wN_stat2, sw;
	char *spec_name = NULL;
	char *specStat = NULL;
	char *filename = NULL;
	int ok, buf=0;
	NXhandle fileid, rw_id;

	filename = (char*)malloc(4096*sizeof(char));
	if(argc < 2){
		fprintf(stderr, "Usage: sgmDataRecorder NXdefintion.xml \n");
	}

    	xmlInitParser();
    	LIBXML_TEST_VERSION
    	xmlKeepBlanksDefault(0);
    	/*parse the file and get the DOM */
    	doc = xmlParseFile(argv[1]);
   	if (doc == NULL) {
        	fprintf(stderr, "There was an error getting the XML file root. \n" );
    	}
	xpathObj = getnodeset(doc, "//*");
	/*Get the name of the first spec application running on system.*/
        spec_name = SPS_GetNextSpec (0);
	if(spec_name == NULL) return -1;
	if(SPS_GetNextArray(spec_name, 0) == NULL){
		fprintf(stderr, "ERROR: core_shm must be initialized for spec_data to run.\n");
		return -1; 
	}
	sleep_dur.tv_sec = 0;
	sleep_dur.tv_nsec = 5000;
	for(;;){
                nanosleep(&sleep_dur, NULL);
		if(SPS_IsUpdated(spec_name, "core_shm")){
			specStat = SPS_GetEnvStr(spec_name, "core_shm", "status");
			if(strcmp(specStat, "0") == 0) sw = 0;
			else if(strcmp(specStat, "1") == 0) sw = 1;
			else if(strcmp(specStat, "2") == 0) sw = 2;
			else if(strcmp(specStat, "3") == 0) sw = 3;
			else if(strcmp(specStat, "4") == 0) sw = 4;
			else if(strcmp(specStat, "100") == 0) sw = 100;
			else sw = 20;
			switch(sw){
				case 0: 
                        	      if(filename != NULL){
					if(strncpy(filename, (SPS_GetEnvStr(spec_name, "core_shm", "filename")), STRMAX) != NULL){
							if(NXopen (filename, NXACC_CREATE5, &fileid) != NX_OK){
							 	fprintf(stderr, "Trouble opening new NeXuS file.\n");
  							}
							else{
								ok = 1;
							}
					}
                        	      }
                        	      else{
                                		fprintf(stderr, "Couldn't initiliaze filename. \n");
                        	      }
                        	      break;


				case 1:
					if(filename != NULL){
						if(strncpy(filename, (SPS_GetEnvStr(spec_name, "core_shm", "filename")), STRMAX) != 0){
							if(NXopen (filename, NXACC_RDWR, &fileid) != NX_OK){
								fprintf(stderr, "There was a problem opening the NeXuS file.\n");
							}
							else{
								ok = 1;
						
							}
						}
					}
					else{
                                		fprintf(stderr, "Couldn't initiliaze filename. \n");
					}
					break;		
	
				case 4: 
                        		if((spec_name != NULL) && (fileid != NULL) && (doc != NULL)){
						if(ok && !wN_stat){
							wN_stat2 = writeNexusData(doc, spec_name, fileid);
						}
						if(wN_stat2){
							fprintf(stderr, 
							"There was an error writing data slab to the NeXuS file. \n");
						}
                        		}
					else{
                                		fprintf(stderr, "Trouble connecting to SPEC or NeXus file pipe. \n");
                        		}
                        		break;
                		case 3:
                        		if((spec_name != NULL) && (fileid != NULL) && (doc != NULL)){
						if(ok){
							wN_stat = writeNexusHead(doc, spec_name, fileid);
						}
						else{
							wN_stat = 1;
						}
						if (wN_stat){ 
							fprintf(stderr,"There was an error writing header to the NeXuS file. \n");
						}
					}
                        		else{
                                		fprintf(stderr, "Trouble connecting to SPEC shared memory. \n");
                        		}
                        		break;
	
				case 2:
					if(fileid != NULL){	
						if(NXclose(&fileid) != NX_OK) printf("Couldn't close the file. \n");
						ok = 0;
					}
					else{
						fprintf(stderr, "There is no file to close.\n");
					}
					break;
        		}       
		if(sw == 100) break;
    		}

	}
  	/*free the document and
    	* Cleanup of XPath data */
    	if(xpathObj != NULL) xmlXPathFreeObject(xpathObj);
    	if(doc != NULL) xmlFreeDoc(doc);
 	if(filename != NULL) free(filename);
    	/*
     	*Free the global variables that may
    	*have been allocated by the parser.
     	*/
    	xmlCleanupParser();
    
    	return 0;
}
#else
int main(void) {
    fprintf(stderr, "Tree support not compiled in\n");
    exit(1);
}
#endif
