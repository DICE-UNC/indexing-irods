include(*path) = !(*path like regex ".*/[.]preview(/.*)?")
ATTR_ID = "data:id"
ATTR_NAME = "data:name"
ATTR_DATA_SIZE = "data:dataSize"
ATTR_THUMB_PREVIEW = "data:thumbPreview"
ATTR_PREVIEW = "data:preview"
ATTR_HAS_VERSION = "data:hasVersion"
ATTR_TITLE = "data:title"
ATTR_DESCRIPTION = "data:description"
ATTR_REPLACES = "data:replaces"
ATTR_REPLACED_BY = "data:replacedBy"
ATTR_RELATED = "data:related"
ATTR_CONTRIBUTOR = "data:contributor"

ERROR_CODE = -1

# AVU Attr's that are mapped to Databook Attr's
# Do not include system attributes such as data:name, data:dataSize, or data:id
databookAttr(*Attr) =
	match *Attr with
		| ATTR_ID => true
		| ATTR_HAS_VERSION => true
		| ATTR_PREVIEW => true
		| ATTR_THUMB_PREVIEW => true
		| ATTR_CONTRIBUTOR => true
		| ATTR_RELATED => true
		| ATTR_REPLACED_BY => true
		| ATTR_REPLACES => true
		| ATTR_TITLE => true
		| ATTR_DESCRIPTION => true
		| *_ => false

databookAttrType(*Attr) =
	match *Attr with 
		| ATTR_ID => "string"
		| ATTR_HAS_VERSION => "string"
		| ATTR_PREVIEW => "string"
		| ATTR_THUMB_PREVIEW => "string"
		| ATTR_CONTRIBUTOR => "string"
		| ATTR_RELATED => "string"
		| ATTR_REPLACED_BY => "string"
		| ATTR_REPLACES => "string"
		| ATTR_TITLE => "string"
		| ATTR_DESCRIPTION => "string"
		| *_ => let *dummy = writeLine("serverLog", "databookAttrType: unsupported data attribute *Attr, return default type 'string'") in
			"string"
constant(*Attr) =
	match *Attr with 
		| ATTR_ID => true
		| ATTR_HAS_VERSION => false
		| ATTR_PREVIEW => false
		| ATTR_THUMB_PREVIEW => false
		| ATTR_CONTRIBUTOR => false
		| ATTR_RELATED => false
		| ATTR_REPLACED_BY => false
		| ATTR_REPLACES => false
		| ATTR_TITLE => false
		| ATTR_DESCRIPTION => false
		| *_ => false
unique(*Attr) =
	match *Attr with
		| ATTR_ID => true
		| ATTR_HAS_VERSION => true
		| ATTR_PREVIEW => false
		| ATTR_THUMB_PREVIEW => false
		| ATTR_CONTRIBUTOR => false
		| ATTR_RELATED => false
		| ATTR_REPLACED_BY => false
		| ATTR_REPLACES => false
		| ATTR_TITLE => true
		| ATTR_DESCRIPTION => true
		| *_ => false
required(*Attr) =
	match *Attr with
		| ATTR_ID => true
		| ATTR_HAS_VERSION => true
		| ATTR_PREVIEW => false
		| ATTR_THUMB_PREVIEW => false
		| ATTR_CONTRIBUTOR => false
		| ATTR_RELATED => false
		| ATTR_REPLACED_BY => false
		| ATTR_REPLACES => false
		| ATTR_TITLE => false
		| ATTR_DESCRIPTION => false
		| *_ => false

# timeStr(*time) = timestrf(*time, "%Y %m %d %H:%M:%S")
# jackson doesn't support this format, just use unix integer serialization
timeStr(*time) = str(double(*time))++"000"

timeStrNow = timeStr(time())

postProcForPut {
	on(include($objPath)) {
		postProcForCreateCommon($objPath, $dataSize, true);
		# preview must be generated after the file is uploaded
		genPreviewGen($objPath, *previewThumbPath, *previewPath);
	}
	or {
		succeed;
	}
}

postProcForCopy {
	on(include($objPath)) {
		postProcForCreateCommon($objPath, $dataSize, true);
		genPreviewGen($objPath, *previewThumbPath, *previewPath);

#		*sourceAssociatedFiles = getAssociatedFiles($srcObjPath);
#		*destAssociatedFiles = getAssociatedFiles($objPath);

		# both list should have the same size
#		*size = size(*sourceAssociatedFiles);

#		for(*i=0;*i<*size;*i=*i+1) {
#			writeLine("serverLog", "copying "++elem(*sourceAssociatedFiles, *i)++" to "++elem(*destAssociatedFiles, *i));
#			msiDataObjCopy(elem(*sourceAssociatedFiles, *i), elem(*destAssociatedFiles, *i), "forceFlag=", *Status);
#		}
	}
	or {
		succeed;
	}
}

# avoid write back as output param
postProcForCreateCommon : input string * input f double * boolean -> integer
postProcForCreateCommon(*objPath, *dataSize, *put) {
	*dataId = _getDataObjId(*objPath, *Found);
	
	if(*Found) {
		(*collName, *dataName) = splitDataObjPath(*objPath);
		*createTime = getFirstResult(
			SELECT DATA_MODIFY_TIME WHERE DATA_NAME = *dataName AND COLL_NAME = *collName,
			"DATA_MODIFY_TIME", 
			*Found
		);	
		*time = datetime(double(*createTime));
		*formattedTimeStr = timeStr(*time);
	
		sendUpdateDataObj($userNameClient, *objPath, str(*dataSize), *formattedTimeStr, *dataId, *put);
	} else {
		genDataObjId(*objPath, *dataId);
		(*collName, *dataName) = splitDataObjPath(*objPath);
		*createTime = getFirstResult(
			SELECT DATA_CREATE_TIME WHERE DATA_NAME = *dataName AND COLL_NAME = *collName,
			"DATA_CREATE_TIME", 
			*Found
		);	
		*time = datetime(double(*createTime));
		*formattedTimeStr = timeStr(*time);
		writeLine("serverLog", "postProcForCreateCommon: new data obj *objPath, *dataSize, *put");
        	sendAddDataObj($userNameClient, *objPath, str(*dataSize), *formattedTimeStr, *put);
	}
}

postProcForDelete {
	on(include($objPath)) {
		sendRemoveDataObj($userNameClient, $objPath);
		foreach(*filename in getAssociatedFiles($objPath)) {
			msiDataObjUnlink(*filename, *Status);
		}
	}
	or {
		succeed;
	}
}

postProcForObjRename(*sourceObject,*destObject) {
	on(include(*sourceObject)) {
		getFirstResult(
			SELECT COLL_NAME WHERE COLL_NAME = (str(*destObject)),
			"COLL_NAME",
			*found
		);
		if(*found) {
	    		sendMoveColl($userNameClient, *sourceObject, *destObject);
		} else {
	    		sendMoveDataObj($userNameClient, *sourceObject, *destObject);
			*sourceAssociatedFiles = getAssociatedFiles(*sourceObject);
			*destAssociatedFiles = getAssociatedFiles(*destObject);

			# both list should have the same size
			*size = size(*sourceAssociatedFiles);

			for(*i=0;*i<*size;*i=*i+1) {
				msiDataObjRename(elem(*sourceAssociatedFiles, *i), elem(*destAssociatedFiles, *i), 0, *Status);
			}
		}
	}
	or {
		succeed;
	}
}

postProcForCollCreate{
	on(include($collName)) {
		genCollId($collName, *Id);
        	sendAddColl($userNameClient, $collName);
	}
	or {
		succeed;
	}
#	or {
#		writeLine("serverLog", "ignored collect $objPath");
#	}
}

preprocForRmColl{
	on(include($collName)) {
		writeLine("serverLog", "acPreprocForRmColl: $collName");
        	sendRemoveColl($userNameClient, $collName);
	}
	or {
		succeed;
	}
}

postProcForOpen{
	on(include($objPath)) {
		writeLine("serverLog", "acPostProcForOpen: $objPath, $writeFlag, dataSize = $dataSize");
		*dataId = getDataObjId($objPath);	
		if($writeFlag=="0") {
			# this is probably a get operation
        		sendGetDataObj($userNameClient, $objPath);
		} else {
			(*collName, *dataName) = splitDataObjPath(*objPath);
			*createTime = getFirstResult(
				SELECT DATA_MODIFY_TIME WHERE DATA_NAME = *dataName AND COLL_NAME = *collName,
				"DATA_MODIFY_TIME", 
				*Found
			);	
			*time = datetime(double(*createTime));
			*formattedTimeStr = timeStr(*time);
			sendUpdateDataObj($userNameClient, $objPath, str($dataSize), *formattedTimeStr, getDataObjId($objPath), false);
		}
		#sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_CLOSE, $userNameClient, $objPath, *dataId);
	}
	or {
		
	}
}

checkMultiplicity(*AName, *objPath, *min, *max) {
	*ret = true;
	(*coll, *data) = splitDataObjPath(*objPath);
	foreach(*r in SELECT count(META_DATA_ATTR_NAME) WHERE META_DATA_ATTR_NAME = *AName AND COLL_NAME = *coll AND DATA_NAME = *data) {
		*mul = int(*r.META_DATA_ATTR_NAME)
		if((*min >=0 && *mul < *min) || (*max >= 0 && *mul > *max)) {
			*ret = false;
		}
		break;
	}
	*ret;
}

preProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, *NAName, *NAValue, *NAUnit) {
	on(include(*ItemName) && databookAttr(*AName)) {
		cut;
		if(constant(*AName) && *Option != "add") {
			failmsg(-1, "cannot perform operation *Option on attribute *AName");
		}
		if(*Option like "mod*" && *NAName != "" && *NAName != "n:*AName") {
			failmsg(-1, "cannot modify name for attribute *AName from *AName to *NAName");
		}
		if(*Option like "add*" && unique(*AName)) {
			if(!checkMultiplicity(*AName, *ItemName, 0, 0)) {
				failmsg(-1, "cannot have more than one value for attribute *AName");
			}
		}
		if(*Option like "rm*" && required(*AName)) {
			if(!checkMultiplicity(*AName, *ItemName, 2, -1)) {
				failmsg(-1, "cannot have more less than one value for attribute *AName");
			}
		}
	}
	or {
		succeed;
	}
}

toJsonType(*IrodsType) {
	if(*IrodsType == "-d") {
		*Type = "DataObject";
	} else if(*IrodsType == "-C") {
		*Type = "Collection";
	} else {
		failmsg(-1, "unsupported object type: *ItemName, *ItemType");
	}
	*Type;
}

objIdFromName(*ItemName, *ItemType, *Option) {
	if(*Option == "rmi") {
			if(*ItemType == "-d") {
				*Id = getDataObjIdById(*ItemName);
			} else if(*ItemType == "-C") {
				*Id = getCollIdById(*ItemName);
			} else {
				failmsg(-1, "unsupported object type: *ItemName, *ItemType");
			}
		} else {
			if(*ItemType == "-d") {
				*Id = getDataObjId(*ItemName);
			} else if(*ItemType == "-C") {
				*Id = getCollId(*ItemName);
			} else {
				failmsg(-1, "unsupported object type: *ItemName, *ItemType");
			}
		}
		*Id;
}

postProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, *NAName, *NAValue, *NAUnit) {
	on(include(*ItemName) && *AName == "data:id") {
		writeLine("serverLog", "skip data:id");
		succeed;
	}
	on(include(*ItemName) && databookAttr(*AName)) {
		cut;
		writeLine("serverLog", "processing *AName for *ItemName");
		*Id = objIdFromName(*ItemName, *ItemType, *Option);
		*Type = toJsonType(*ItemType);
		# need to check *AValue format
		*DatabookName=triml(*AName, ":");
		writeLine("serverLog", "*Option, *Id, *DatabookName, *AValue, *AUnit");
		*IdJson = jsonEncode(*Id);
		*DatabookNameJson = jsonEncode(*DatabookName);
		*AValueJson = jsonEncode(*AValue);
# unit should be empty
		*AUnitJson = jsonEncode(*AUnit);
		
		if(*Option like "add*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "union",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
							    "*DatabookNameJson" : "*AValueJson"
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_ADD;
		} else if(*Option like "rm*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "diff",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
								"*DatabookNameJson" : "*AValueJson"
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_REMOVE;
		} else if(*Option like "set*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "modify",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
								"*DatabookNameJson" : "*AValueJson"
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_MODIFY;
		} else if(*Option like "mod*") {
		writeLine("serverLog", "*NAValue, *NAUnit");
			*NAValueJson = jsonEncode(triml(*NAValue, ":"));
			*NAUnitJson = jsonEncode(triml(*NAUnit,":"));
			*msg='
			{
				"messages" : [
					{
						"operation" : "modify",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
							    "*DatabookNameJson" : "*NAValueJson"
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_MODIFY;
		} else {
			failmsg(-1, "unsupported option: *Option, *ItemName");
		}
    	amqpSend("localhost", "metaQueue", *msg);
		# send access log	
		sendAccess(*accessType, $userNameClient, *Id, *Type, timeStrNow(), "*AName *accessType");
	}
	on(include(*ItemName)) {
		cut;
		writeLine("serverLog", "processing *AName for *ItemName");
		*Id = objIdFromName(*ItemName, *ItemType, *Option);
		*Type = toJsonType(*ItemType);
		# need to check *AValue format
		*IdJson = jsonEncode(*Id);
		*DatabookNameJson = jsonEncode(*AName);
		*AValueJson = jsonEncode(*AValue);
		*AUnitJson = jsonEncode(*AUnit);
		
		if(*Option like "add*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "union",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
								"metadata" : [
									{
										"attribute" : "*DatabookNameJson",
										"value" : "*AValueJson",
										"unit" : "*AUnitJson"
									}
								]
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_ADD;
		} else if(*Option like "rm*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "diff",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
								"metadata" : [
									{
										"attribute" : "*DatabookNameJson",
										"value" : "*AValueJson",
										"unit" : "*AUnitJson"
									}
								]
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_REMOVE;
		} else if(*Option like "set*") {
			*msg='
			{
				"messages" : [
					{
						"operation" : "modify",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
							}, {
								"type" : "*Type",
								"metadata" : [
									{
										"attribute" : "*DatabookNameJson",
										"value" : "*AValueJson",
										"unit" : "*AUnitJson"
									}
								]
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_MODIFY;
		} else if(*Option like "mod*") {
			*NAValueJson = jsonEncode(triml(*NAValue, ":"));
			*NAUnitJson = jsonEncode(triml(*NAUnit,":"));
			*msg='
			{
				"messages" : [
					{
						"operation" : "modify",
						"hasPart" : [
							{
								"type" : "*Type",
								"uri" : "*IdJson"
								"metadata" : [
									{
										"attribute" : "*DatabookNameJson",
										"value" : "*AValueJson",
										"unit" : "*AUnitJson"
									}
								]
							}, {
								"type" : "*Type",
								"metadata" : [
									{
										"attribute" : "*DatabookNameJson",
										"value" : "*NAValueJson",
										"unit" : "*NAUnitJson"
									}
								]
							}
						]
						
					}
				]
			}';
			*accessType=ACCESS_TYPE_METADATA_MODIFY;
		} else {
			failmsg(-1, "unsupported option: *Option, *ItemName");
		}
    	amqpSend("localhost", "metaQueue", *msg);
		# send access log	
		sendAccess(*accessType, $userNameClient, *Id, *Type, timeStrNow(), "*AName *accessType");
	}
	or { 
		succeed;
	}
}

sendAddDataObj(*userName, *objPath, *dataSize, *dateCreated, *put) {
        *coll=jsonEncode(trimr(str(*objPath), "/"));
	*collId=getCollId(*coll);
	*collIdJson =jsonEncode(*collId);
	*objId=getDataObjId(*objPath);
	*objIdJson=jsonEncode(*objId);
	*objPathJson=jsonEncode(str(*objPath));
	*userNameJson=jsonEncode(*userName);
	*dateCreatedJson = timeStr(*dateCreated); 
	*msg='
	{
		"messages" : [
			{
				"operation" : "create",
				"hasPart" : [
					{
						"type" : "DataObject",
						"uri" : "*objIdJson",
						"label" : "*objPathJson",
						"dataSize" : "*dataSize",
						"created" : "*dateCreated",
						"submitted" : "*dateCreated",
						"owner" : {
							"uri": "*userNameJson"
						},
						"partOf" : [
							{
								"type" : "Collection",
								"uri" : "*collIdJson"
							}
						]
					}
				]
			}
		]
	}
	';
    amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(if *put then ACCESS_TYPE_DATA_OBJ_PUT else ACCESS_TYPE_DATA_OBJ_CREATE, *userName, *objPath, *objId);
}

sendAddColl(*userName, *collPath) {
        *coll=trimr(str(*collPath), "/");
	if(*coll=="") {
		*collId="root";	
	} else {
		*collId=getCollId(*coll);
	}
	*collIdJson = jsonEncode(*collId);
	*objId=getCollId(*collPath)
	*objIdJson = jsonEncode(*objId);
	*collPathJson = jsonEncode(str(*collPath));
	*msg='
	{
		"messages" : [
			{
				"operation" : "create",
				"hasPart" : [
					{
						"type" : "Collection",
						"uri" : "*objIdJson",
						"partOf" : [ {
							"type": "Collection",
							"uri": "*collIdJson"
						} ],
						"label" : "*collPathJson"
					}
				]
			}
		]
	}';
	#writeLine("stdout", "message = *msg");
    amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(ACCESS_TYPE_COLL_CREATE, *userName, str(*collPath), *objId);
}

sendUpdateDataObj(*userName, *objPath, *dataSize, *submitted, *dataId, *put) {
	*dataIdJson = jsonEncode(*dataId);
	*msg='
	{
		"messages" : [
			{
				"operation" : "modify",
				"hasPart" : [
					{
						"type" : "DataObject",
						"uri" : "*dataIdJson"
					}, {
						"type" : "DataObject",
						"dataSize" : *dataSize,
						"submitted" : "*submitted"
					}
				]
			}
		]
	}';
	amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(if *put then ACCESS_TYPE_DATA_OBJ_OVERWRITE else ACCESS_TYPE_DATA_OBJ_UPDATE, *userName, *objPath, *dataId);
}

sendRemoveDataObj(*userName, *objPath) {
#    *coll=trimr(*objPath, "/");
#	*collId=getCollId(*coll);
#	*collIdJson = jsonEncode(*collId);
	*objId=getDataObjId(*objPath);
	*objIdJson = jsonEncode(*objId);
	*msg = '
	{
		"messages": [
			{
				"operation" : "delete",
				"hasPart" : [
					{
						"type" : "DataObject",
						"uri" : "*objIdJson",
					}
				]				
			}
		]
	}
	';

    amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_REMOVE, *userName, *objPath, *objId);
}

sendRemoveColl(*userName, *objPath) {
#    *coll=trimr(*objPath, "/");
#	*collId=getCollId(*coll);
#	writeLine("serverLog", "collId = *collId");
	*objId=getCollId(*objPath);
	*objIdJson = jsonEncode(*objId);
	*msg = '
	{
		"messages": [
			{
				"operation" : "delete",
				"hasPart" : [
					{
						"type" : "Collection",
						"uri" : "*objIdJson",
					}
				]				
			}
		]
	}
	';

	writeLine("serverLog", "objId = *objId");
    amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(ACCESS_TYPE_COLL_DELETE, *userName, *objPath, *objId);
}

sendGetDataObj(*userName, *objPath) {
	*objId=getDataObjId(*objPath);
	# send access log	
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_GET, *userName, *objPath, *objId);
}

sendMoveDataObj(*userName, *objPathOld, *objPathNew) {
    *collOld=trimr(*objPathOld, "/");
    *collNew=trimr(*objPathNew, "/");
	*collOldId=getCollId(*collOld);
	*collOldIdJson = jsonEncode(*collOldId);
	*collNewId=getCollId(*collNew);
	*collNewIdJson=jsonEncode(*collNewId);
	*objId=getDataObjId(*objPathNew);
	*objIdJson=jsonEncode(*objId);
	*objPathOldJson = jsonEncode(*objPathOld);
	*objPathNewJson = jsonEncode(*objPathNew);
	*msg = '
	{
		"messages": [
			{
				"operation" : "modify",
				"hasPart" : [
					{
						"type" : "DataObject",
						"uri" : "*objIdJson",
						"partOf" : [
							{
								"type" : "Collection",
								"uri" : "*collOldIdJson"
							}
						]
					}, {
						"type" : "DataObject",
						"label" : "*objPathNewJson",
						"partOf" : [
							{
								"type" : "Collection",
								"uri" : "*collNewIdJson"
							}
						]
					}
				]				
			}
		]
	}
	';
    amqpSend("localhost", "metaQueue", *msg);
	# send access log	
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_MOVE, *userName, (*objPathOld, *objPathNew), *objId);
}

sendMoveColl(*userName, *objPathOld, *objPathNew) {
        *collOld=trimr(*objPathOld, "/");
        *collNew=trimr(*objPathNew, "/");
	*collOldId=getCollId(*collOld);
	*collOldIdJson = jsonEncode(*collOldId);
	*collNewId=getCollId(*collNew);
	*collNewIdJson = jsonEncode(*collNewId);
	*objId=getCollId(*objPathNew);
	*objIdJson = jsonEncode(*objId);
	*objPathNewJson = jsonEncode(*objPathNew);
	*msg = '
	{
		"messages": [
			{
				"operation" : "modify",
				"hasPart" : [
					{
						"type" : "Collection",
						"uri" : "*objIdJson",
						"partOf" : [
							{
								"type" : "Collection",
								"uri" : "*collOldIdJson"
							}
						]
					}, {
						"type" : "Collection",
						"label" : "*objPathNewJson",
						"partOf" : [
							{
								"type" : "Collection",
								"uri" : "*collNewIdJson"
							}
						]
					}
				]				
			}
		]
	}
	';
	# send access log
	sendAccessDataObj(ACCESS_TYPE_COLL_MOVE, *userName, (*objPathOld, *objPathNew), *objId);
}

sendCollectionStructure(*path) {
	*dataObjs = SELECT DATA_NAME, COLL_NAME, DATA_SIZE WHERE COLL_NAME like "*path%";
    foreach(*dataObj in *dataObjs) {
		writeLine("stdout", *dataObj);
		*dataName = *dataObj.DATA_NAME;
		*dataSize = *dataObj.DATA_SIZE;
		*collName = *dataObj.COLL_NAME;
		*objPath = /*collName/*dataName;
		sendAddDataObj("sync", *objPath, *dataSize, timeStrNow(), false);
    }
	*dataObjs = SELECT COLL_NAME WHERE COLL_NAME like "*path%";
    foreach(*dataObj in *dataObjs) {
		*objPath = path(*dataObj.COLL_NAME);
		sendAddColl("sync", *objPath);
    }
}

# access type: 
# automatic: 
ACCESS_TYPE_DATA_OBJ_MOVE = "data object move"
ACCESS_TYPE_DATA_OBJ_REMOVE = "data object remove" # remove is issued automatically from peps
ACCESS_TYPE_COLL_MOVE = "collection move"
ACCESS_TYPE_DATA_OBJ_UPDATE = "data object update" # write close
ACCESS_TYPE_DATA_OBJ_OVERWRITE = "data object overwrite" # overwriting put/copy
ACCESS_TYPE_DATA_OBJ_PUT = "data object put" # used only for non overwriting put/copy, a combination of CREATE, WRITE, UPDATE
ACCESS_TYPE_DATA_OBJ_GET = "data object get" # read-only close
ACCESS_TYPE_METADATA_ADD = "metadata add"
ACCESS_TYPE_METADATA_REMOVE = "metadata remove"
ACCESS_TYPE_METADATA_MODIFY = "metadata modify"
# manual: 
ACCESS_TYPE_RULE = "rule"
ACCESS_TYPE_MICROSERVICE = "microservice"
ACCESS_TYPE_DATA_OBJ_READ = "data object read"
ACCESS_TYPE_DATA_OBJ_WRITE = "data object write"
ACCESS_TYPE_DATA_OBJ_OPEN = "data object open"
ACCESS_TYPE_DATA_OBJ_CLOSE = "data object close" # current this is unused, either an UPDATE, PUT, or GET is used instead
ACCESS_TYPE_DATA_OBJ_CREATE = "data object create"
ACCESS_TYPE_DATA_OBJ_DELETE = "data object delete" # current this is unused, when a data obj is moved to trash, MOVE is used, and when deleted from trash, REMOVE is used
ACCESS_TYPE_COLL_READ = "collection read"
ACCESS_TYPE_COLL_WRITE = "collection write"
ACCESS_TYPE_COLL_CREATE = "collection create"
ACCESS_TYPE_COLL_DELETE = "collection delete"

sendAccess(*AccessType, *UserName, *DataId, *DataType, *Time, *Description) {
	sendAccessWithSession(*AccessType, *UserName, *DataId, *DataType, *Time, *Description, getGlobalSessionId());
}

sendAccessWithSession(*AccessType, *UserName, *DataId, *DataType, *Time, *Description, *SessionId) {
	*AccessId = jsonEncode(genAccessId(*AccessType, *UserName, *DataId, *Time, *Description));
	*UserNameJson = jsonEncode(*UserName);
	*DescriptionJson = jsonEncode(*Description);
	*DataIdJson = jsonEncode(*DataId);
	*SessionIdJson = jsonEncode(*SessionId);
	*msg='
	{
		"messages" : [ {
			"operation" : "create",
			"hasPart" : [
				{
					"type" : "Access",
					"uri" : "*AccessId",
					"title" : "*AccessType",
					"linkingDataEntity" : [
						{
							"uri": "dataEntityLink*Time",
							"dataEntity" : {
								"type" : "*DataType",
								"uri": "*DataIdJson"
							}
						}
					],
					"linkingUser" : [
						{
							"uri": "userLink*Time",
							"user" : {
								"uri": "*UserNameJson"
							}
						}	
					],
					"created" : "*Time",
					"description" : "*DescriptionJson",
					"partOf" :  [ {
						"type": "Session",
						"uri": "*SessionIdJson"
					} ]
				}
			]
		} ]
	}
	'; # change *AccessId to URI
    amqpSend("localhost", "metaQueue", *msg);	
}

sendAction(*AccessType, *UserName, *ActionId, *TimeStart, *TimeEnd, *Description) {
	sendActionWithSession(*AccessType, *UserName, *ActionId, *TimeStart, *TimeEnd, *Description, getGlobalSessionId());
}

sendActionWithSession(*AccessType, *UserName, *ActionId, *TimeStart, *TimeEnd, *Description, *SessionId) {
	*AccessId = jsonEncode(genAccessId(*AccessType, *UserName, *ActionId, *TimeStart, *Description));
	*UserNameJson = jsonEncode(*UserName);
	*DescriptionJson = jsonEncode(*Description);
	*SessionIdJson = jsonEncode(*SessionId);
	*msg='
	{
		"messages" : [ {
			"operation" : "create",
			"hasPart" : [
				{
					"type" : "Access",
					"uri" : "*AccessId",
					"title" : "*AccessType",
					"linkedUserEntity" : [
						{
							"userEntityLink" : "*UserNameJson"
						}	
					],
					"created" : "*TimeStart",
					*finished" : "*TimeEnd",
					"description" : "*DescriptionJson",
					"partOf" :  [ "*SessionIdJson" ]
				}
			]
		}
	}
	'; # change *AccessId to URI
    amqpSend("localhost", "metaQueue", *msg);	
}

splitDataObjPath(*objPath) {
	*objPathStr = str(*objPath);
	*objPathStrLen = strlen(*objPathStr);
	*collName = trimr(*objPathStr, "/");
	*dataNameLen = *objPathStrLen-strlen(*collName)-1;
	*dataName = substr(*objPathStr, *objPathStrLen - *dataNameLen, *objPathStrLen);
	(*collName, *dataName);
}

getDataObjIdById(*id) {
	foreach(*r in SELECT DATA_NAME, COLL_NAME WHERE DATA_ID = *id) {
	      *objPath = *r.COLL_NAME ++ "/" ++ *r.DATA_NAME;
	      break;
	}
	
	getDataObjId(*objPath, *Found);
}

getCollIdById(*id) {
	foreach(*r in SELECT COLL_NAME WHERE COLL_ID = *id) {
	      *collPath = *r.COLL_NAME;
	      break;
	}
	
	getCollId(*collPath, *Found);
}

getCollId(*coll) {
	writeLine("serverLog", "getCollId of *coll");
	*Id = getFirstResult(
		SELECT META_COLL_ATTR_VALUE WHERE COLL_NAME = (str(*coll)) AND META_COLL_ATTR_NAME = ATTR_ID,
		"META_COLL_ATTR_VALUE",
		*Found
	);
	if(!*Found) {
		genCollId(*coll, *Id);
	}
	writeLine("serverLog", *Id);
	*Id;
}

_getDataObjId(*objPath, *Found) {
	(*collName, *dataName) = splitDataObjPath(*objPath);
	writeLine("serverLog", "getObjId of *objPath, *dataName, *collName");
	*Id = getFirstResult(
		SELECT META_DATA_ATTR_VALUE WHERE DATA_NAME = *dataName AND COLL_NAME = *collName AND META_DATA_ATTR_NAME = ATTR_ID,
		"META_DATA_ATTR_VALUE",
		*Found
	);
	*Id;
}

getDataObjId(*objPath) {
	*Id = _getDataObjId(*objPath, *Found);
	if(!*Found) {
		genDataObjId(*objPath, *Id);
	}
	writeLine("serverLog", *Id);
	*Id;
}

genDataObjId(*path, *Id) {
	*Id = str(*path) ++ "@" ++ str(double(time()));
	addMetaAV(*path, ATTR_ID, *Id, "-d");
}

genCollId(*path, *Id) {
#	writeLine("serverLog", "genCollId: *path");
	*Id = str(*path) ++ "@" ++ str(double(time()));
	addMetaAV(*path, ATTR_ID, *Id, "-C");
}

genAccessId(*AccessType, *UserName, *DataId, *Time, *Description) {
	# need a more sophisticated method
	*AccessId = *DataId++"a"++str(double(time()))++replace(*AccessType, " ", "_");
	*AccessId;
}

getFileType(*objPath) {
	(*collName, *dataName) = splitDataObjPath(*objPath);
	*phyPath = getPhyPath(*collName, *dataName);
	writeLine("serverLog", "getFileType: *objPath, *phyPath");
	*e = execCmd("file", list("-b", *phyPath), *status);
	if(*e < 0) {
		*t = "";
	} else {
		msiGetStdoutInExecCmdOut(*status, *out);
		*l = strstr(*out, " ");
		if(*l == -1) {
			*t = "";
		} else {
			*t = substr(*out, 0, *l);
		}
	}
	*t;
}

canGenPreview(*objPath) = 
	let *ext = getFileType(*objPath) in
	*ext == "JPEG" || *ext == "PNG" || *ext == "GIF" || *ext == "PDF"

genPreviewGen(*objPath, *previewThumbPath, *previewPath) {
	on(getFileType(*objPath)=="JPEG") {
		genPicPreview(*objPath, *previewThumbPath, *previewPath);
	}
	on(getFileType(*objPath)=="PNG") {
		genPicPreview(*objPath, *previewThumbPath, *previewPath);
	}
	on(getFileType(*objPath)=="GIF") {
		genPicPreview(*objPath, *previewThumbPath, *previewPath);
	}
	on(getFileType(*objPath)=="PDF") {
		genPdfPreview(*objPath, *previewThumbPath, *previewPath);
	}
	or {
		writeLine("serverLog", "genPreview: unsupported file type for *objPath, ext = "++getFileType(*objPath));
		*previewPath = ""; # set preview path to empty		
	}
}

join(*list) {
	*out = "";
	foreach(*a in *list) {
		*out = "*out*a\n";
	}
	substr(*out, 0, strlen(*out)-1);
}

getFileExt(*objPath) = 
	let *baselen = strlen(trimr(*objPath, ".")) in
	let *objPathLen = strlen(*objPath) in
	if *baselen == *objPathLen then "" else substr(*objPath, *baselen+1, *objPathLen)

getAssociatedFiles(*objPath) =
	let (*collName, *dataName) = splitDataObjPath(*objPath) in
	let *p = getPhyPath(*collName, *dataName) in
	if canGenPreview(*objPath) then
		let (*previewPath, *previewPathTmp, *previewThumbPath, *previewThumbPathTmp) = getPreviewPaths(*collName, *dataName) in
		list(*previewPath, *previewThumbPath)
	else
		list()

getPreviewPaths(*collName, *dataName) = 
	let *timeStr = str(double(time())) in (
		str(/*collName/.preview/*dataName.preview.jpeg),
		str(/tmp/*dataName*timeStr.preview.jpeg),
		str(/*collName/.preview/*dataName.previewThumb.jpeg),
		str(/tmp/*dataName*timeStr.previewThumb.jpeg)
	)

getPhyPath(*collName, *dataName) =
	getFirstResult(
		SELECT DATA_PATH WHERE DATA_NAME = *dataName AND COLL_NAME = *collName,
		"DATA_PATH",
		*Found
	)

genPreview(*objPath, *previewThumbPath, *previewPath, *script) {
	(*collName, *dataName) = splitDataObjPath(*objPath);

	*previewCollection = /*collName/.preview;
	createCollIfNotExist(*previewCollection);

	*objPhyPath = getPhyPath(*collName, *dataName);
	(*previewPath, *previewPathTmp, *previewThumbPath, *previewThumbPathTmp) = getPreviewPaths(*collName, *dataName);

	writeLine("serverLog", "generating preview for *objPath, ext = "++getFileExt(*objPath) ++", phypath = *objPhyPath");
		*errcode = execCmd(*script, list(*objPhyPath, *previewThumbPath, *previewThumbPathTmp, *previewPath, *previewPathTmp), *status);

		if(*errcode < 0) {
			writeLine("serverLog", "convert thumbnail error *errcode"); # : out = *out, err = *err");
		}

		# send thumb link first, the preview attribute triggers a caching on the VIVO side
		addMetaAV(*objPath, ATTR_THUMB_PREVIEW, *previewThumbPath, "-d");
		addMetaAV(*objPath, ATTR_PREVIEW, *previewPath, "-d");
#	genPreviewDelay(*objPath, *objPhyPath, *previewThumbPath, *previewThumbPathTmp, *previewPath, *previewPathTmp, *script)
}

genPreviewDelay(*objPath, *objPhyPath, *previewThumbPath, *previewThumbPathTmp, *previewPath, *previewPathTmp, *script) {
	delay("<PLUSET>0s</PLUSET>") {
	
		*errcode = execCmd(*script, list(*objPhyPath, *previewThumbPath, *previewThumbPathTmp, *previewPath, *previewPathTmp), *status);

		if(*errcode < 0) {
			writeLine("serverLog", "convert thumbnail error *errcode"); # : out = *out, err = *err");
		}

		# send thumb link first, the preview attribute triggers a caching on the VIVO side
		addMetaAV(*objPath, ATTR_THUMB_PREVIEW, *previewThumbPath, "-d");
		addMetaAV(*objPath, ATTR_PREVIEW, *previewPath, "-d");
	}
}

genPicPreview(*objPath, *previewThumbPath, *previewPath) {
	genPreview(*objPath, *previewThumbPath, *previewPath, "convertThumbnail.sh")
}

genPdfPreview(*objPath, *previewThumbPath, *previewPath) {
	genPreview(*objPath, *previewThumbPath, *previewPath, "convertPdfThumbnail.sh")
}

createCollIfNotExist(*path) {
	getFirstResult(
		SELECT COLL_NAME WHERE COLL_NAME = (str(*path)),
		"COLL_NAME",
		*found
	);
	if(! *found) {
		msiCollCreate(*path, "0", *status);
	}
}

getKVP(*KVP, *Key) {
	msiGetValByKey(*KVP, *Key, *Val);
	*Val;
}

getFirstResult(*rs, *column, *found) {
	# dummy code to force the type of *RS
	if(false) {
		*rs = SELECT DATA_PATH WHERE DATA_NAME="rods";
	}
#	writeLine("serverLog", "get first result");
#	writeLine("serverLog", type(*rs));
	*found = false;
	*id = "";
	foreach(*a in *rs) {
		*id = getKVP(*a, *column);
		*found = true;
		break;
	}
	*id;
}

addMetaAV(*objPath, *attr, *value, *type) {
#	writeLine("serverLog", "addMetaAV: *objPath, *attr, *value, *type");
	*kvp.*attr=*value;
	msiAssociateKeyValuePairsToObj(*kvp, *objPath, *type);
	
	# don't send id because the create message hasn't been sent yet
	if(*attr != ATTR_ID) {
		*databookName=triml(*attr, ":");
		if(*type == "-d") {
			*id = getDataObjId(*objPath);
		} else if(*type == "-C") {
			*id = getCollId(*objPath);
		}
		*type = if *type=="-d" then "DataObject" else "Collection";
		*attrJson = jsonEncode(*attr);
		*valueJson = jsonEncode(*value);
		*unitJson = "";
		*msg='{
			"messages": [
				{
					"operation": "union",
					"hasPart": [
						{
							"type": "*type",
							"uri": "*id"
						},
						{
							"type": "*type",
							"metadata": [
								{
									"attribute": "*attrJson",
									"value": "*valueJson",
									"unit": "*unitJson"
								}
							]
						}
					]
				}
			]
		}';
		amqpSend("localhost", "metaQueue", *msg);
    }
}

execCmd(*cmd, *args, *status) {
	*argStr = "";
	foreach(*arg in *args) {
		*argStr = *argStr++execCmdArg(*arg)++" ";
	}
	*argStr = substr(*argStr, 0, strlen(*argStr) - 1);
	writeLine("serverLog", *cmd ++ " " ++ *argStr);
	*e = errorcode(msiExecCmd(*cmd, *argStr, "null", "null", "null", *status));
	#if(*e < 0) {
	#	msiGetStderrInExecCmdOut(*status, *err);
	#	writeLine("serverLog", "execCmd: *err");
	#}
	*e;
}

# wrapper rules for data access

sendAccessDataObjCurrentUser(*accessType, *objPath, *dataId) {
	sendAccessDataObj(*accessType, $userNameClient, *objPath, *dataId);
}

# *objPath can be either a single path or a pair of paths (for move)
sendAcesssDataObj : string * string * ? * string -> integer 
sendAccessDataObj(*accessType, *userId, *objPath, *dataId) {
	if(*accessType == ACCESS_TYPE_DATA_OBJ_CREATE) {
		 *description = "<img src='images/icons/create.png' />*objPath";
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_OPEN) {
		 *description = "<img src='images/icons/open.png' />*objPath";
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_READ) {
		 *description = "<img src='images/icons/read.png' />*objPath";
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_WRITE) {
		 *description = "<img src='images/icons/write.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_CLOSE) {
		 *description = "<img src='images/icons/close.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_DELETE) {
		 *description = "<img src='images/icons/delete.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_UPDATE) {
		 *description = "<img src='images/icons/update.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_OVERWRITE) {
		 *description = "<img src='images/icons/overwrite.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_PUT) {
		 *description = "<img src='images/icons/put.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_GET) {
		 *description = "<img src='images/icons/get.png' />*objPath"; 
	} else if(*accessType == ACCESS_TYPE_DATA_OBJ_MOVE) {
		# split objPath
		(*objPathOld, *objPathNew) = *objPath;
		 *description = "*objPathOld<img src='images/icons/move.png' />*objPathNew"; 
	} else if(*accessType == ACCESS_TYPE_COLL_MOVE) {
		# split objPath
		(*objPathOld, *objPathNew) = *objPath;
		 *description = "*objPathOld<img src='images/icons/move.png' />*objPathNew"; 
	} else {
		 *description = "*objPath *accessType";
	}
	sendAccess(*accessType, *userId, *dataId, "DataObject", timeStrNow(), *description);
}

# this closes the data obj immediately to trigger event
dataObjCreate(*objPath, *options, *dataObj) {
	msiDataObjCreate(*objPath, *options, *desc);
	postProcForCreateCommon(*objPath, 0, false);
	*dataId = getDataObjId(*objPath);
	*dataObj = (*desc, *objPath, *dataId);
}

dataObjLseek(*dataObj, *off, *base, *status) {
	(*desc, *objPath, *dataId) = *dataObj;
	msiDataObjLseek(*desc, *off, *base, *status);
}

dataObjOpen(*objPath, *options, *dataObj) {
	if (*options == "") {
 		*kvpStr = *objPath;
	} else {
		*kvpStr = "objPath=*objPath++++*options";
	}
	msiDataObjOpen(*kvpStr, *desc);
	*dataId = getDataObjId(*objPath);
	*dataObj = (*desc, *objPath, *dataId);
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_OPEN, $userNameClient, *objPath, *dataId);
}

dataObjRead(*dataObj, *len, *buf) {
	(*desc, *objPath, *dataId) = *dataObj;
	msiDataObjRead(*desc, *len, *buf);
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_READ, $userNameClient, *objPath, *dataId);
}

dataObjWrite(*dataObj, *buf, *len) {
	(*desc, *objPath, *dataId) = *dataObj;
	msiDataObjWrite(*desc, *buf, *len);
	sendAccessDataObj(ACCESS_TYPE_DATA_OBJ_WRITE, $userNameClient, *objPath, *dataId);
}

dataObjClose(*dataObj, *status) {
	(*desc, *objPath, *dataId) = *dataObj;
	msiDataObjClose(*desc, *status);
	# the close action has triggered peps, don't send access here
}
dataObjUnlink(*objPath, *options, *status) {
	if (*options == "") {
 		*kvpStr = *objPath;
	} else {
		*kvpStr = "objPath=*objPath++++*options";
	}
	*dataId = getDataObjId(*objPath);
	msiDataObjUnlink(*kvpStr, *status);
	# peps have already been triggered by msiDataObjUnlink
	# sendAccessDataObjCurrentUser(ACCESS_TYPE_DATA_OBJ_DELETE, *objPath, *dataId);
}

sendActionCurrentUser(*AccessType, *ActionId, *TimeStart) {
	if(*AccessType == ACCESS_TYPE_RULE) {
		*description = "<img src='images/icons/rule.png' />*ActionId";
	} else if(*AccessType == ACCESS_TYPE_MICROSERVICE) {
		*description = "<img src='images/icons/microservice.png' />*ActionId";
	}	
	sendAction(*AccessType, $userNameClient, *ActionId, *TimeStart, timeStrNow(), *description)
}

actionStart(*actionId) = (*actionId, timeStrNow())

ruleStart(*ruleId) = actionStart(*ruleId)

ruleFinish(*rule) {
	let (*actionId, *timeStart) = *rule in
	sendActionCurrentUser(ACCESS_TYPE_RULE, *actionId, *timeStart);
}

ruleError(*rule) = ruleFinish(*rule)

microserviceStart(*microserviceId) = actionStart(*microserviceId)

microserviceFinish(*rule) {
	let (*actionId, *timeStart) = *rule in
	sendActionCurrentUser(ACCESS_TYPE_MICROSERVICE, *actionId, *timeStart);
}

microserviceError(*rule) = microserviceFinish(*rule)


replace(*str, *src, *tgt) {
	if(*src == "") {
		*str;
	} else {
	*strlen = strlen(*str);
	*srclen = strlen(*src);
	*newstr = "";
	*cp = 0;
	for(*i=0; *i<*strlen-*srclen+1; *i=*i+1) {
		if(substr(*str, *i, *i+*srclen) == *src) {
			*newstr = *newstr ++ substr(*str, *cp, *i) ++ *tgt;
			*i = *i+*srclen-1;
			*cp = *i+1;
		}
	}
	*newstr = *newstr ++ substr(*str, *cp, *strlen);
	*newstr;
	}
}

strstr(*str, *src) {
	*strlen = strlen(*str);
	*srclen = strlen(*src);
	*l = -1;
	for(*i=0; *i<*strlen-*srclen+1; *i=*i+1) {
		if(substr(*str, *i, *i+*srclen) == *src) {
			*l = *i;
			break;
		}
	}
	*l;
}

jsonEncode(*str) {
	*str;
}
