acPostProcForPut {
	postProcForPut;
}
acPostProcForCopy {
	postProcForCopy;
}
acPostProcForDelete {
	postProcForDelete;
}

acPostProcForObjRename(*sourceObject,*destObject) {
	postProcForObjRename(*sourceObject, *destObject);
}

acPostProcForOpen{
	postProcForOpen;
}

acPostProcForCollCreate{
	postProcForCollCreate;
}

acPreprocForRmColl{
	preprocForRmColl;
}

acPreProcForModifyAVUMetadata(*Option, *ItemType, *SrcItemName, *TgtItemName) { 
	preProcForModifyAVUMetadata(*Option, *ItemType, *SrcItemName, *TgtItemName, "", "", "", "", "");
}

acPreProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit) { 
	preProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, "", "", "");
}

acPreProcForModifyAVUMetadata(*Option,*ItemType,*ItemName,*AName,*AValue,*AUnit, *NAName, *NAValue, *NAUnit) { 
	preProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, *NAName, *NAValue, *NAUnit);
}

acPostProcForModifyAVUMetadata(*Option, *ItemType, *SrcItemName, *TgtItemName) { 
	postProcForModifyAVUMetadata(*Option, *ItemType, *SrcItemName, *TgtItemName, "", "", "", "", "");
}

acPostProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit) { 
	postProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, "", "", "");
}

acPostProcForModifyAVUMetadata(*Option,*ItemType,*ItemName,*AName,*AValue,*AUnit, *NAName, *NAValue, *NAUnit) { 
writeLine("serverLog", *NAValue);
		postProcForModifyAVUMetadata(*Option, *ItemType, *ItemName, *AName, *AValue, *AUnit, *NAName, *NAValue, *NAUnit);
}

acPostProcForModifyCollMeta { }

acPostProcForModifyDataObjMeta { }

